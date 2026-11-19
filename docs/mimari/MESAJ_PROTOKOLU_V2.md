# Sim ↔ C++ mesaj protokolü v2 (taslak, tartışma için)

Durum: **uygulandı** (2026-11-12): Python sim (`src/simulator/`), C++ io (`src/otopilot/io/`), görüntüleyici. Uçtan uca doğrulandı. Örnek mesajlar: `docs/mimari/mesaj_sablonlari/tcp/`.

## Kullanıcı kararları (2026-11-12)

| Konu | Karar |
|---|---|
| Füze tanımı | `scenario.yaml`'da her uçağın `ammo`'su altında füze tipi ve adedi (ör. `aim120c5: 4`, `aim9x: 2`). Sim profilleri `data/missiles/<ad>.yaml`'dan okur. C++ aynı adla kendi `missile_profiles`'ından uyum profilini seçer. `hello` bu listeyi C++'a taşır. |
| `request_id` | Evet |
| `load_limit_reached` | Şimdilik yok |
| Olaylar | Açıklandı (aşağıdaki liste), karar bekliyor |
| Faz adları | Evet, kayıtta ad |
| Birimler | Evet, mesajda SI |
| Düşman füzesi ve uçağı | Faz 5'e kadar gerçek konum ve hız (`source: "truth"`) |
| Eski protokol | Desteklenmeyecek; eski `src/sim` bu protokole taşınmayacak |
| Sıra | Belgedeki uygulama sırası onaylandı |

## Neden

Mevcut protokol BVRGym zamanında kuruldu. O zaman sim yalnız konum biliyordu; pitbull, güdüm desteği, isabet gibi durumları C++ tahmin ediyordu. Yeni sim bunların gerçeğini biliyor. Mevcut sorunlar:

| Sorun | Bugün | Etkisi |
|---|---|---|
| Füze durumu tahmin ediliyor | C++ `is_pitbull`'u mesafeden, `is_guided`'ı gimbal'dan tahmin ediyor | Sim'deki gerçekle uyuşmayabilir |
| Füze eşleştirme tahmin | C++ atış emrinden sonra sim füzesini "eşleşmemiş en eski atış" diye bağlıyor (`state_reader.cpp`) | Aynı adımda iki atışta karışabilir |
| Atış sonucu yok | `fire()` reddederse C++ bilmiyor | C++ füzeyi atılmış sayıyor |
| Füze tipi iki yerde | C++ loadout'u kendi profil tablosundan, sim yaml'dan alıyor | Uyuşmazsa C++ hata veriyor |
| Olaylar yok | C++ durum değişimlerini kendisi karşılaştırarak buluyor | Kaçırma ya da çift sayma riski |
| Herkes her şeyi görüyor | Her iki uçak da her şeyin gerçeğini alıyor | Gerçekçilik yok; sensör katmanı (Faz 5) eklenemez |
| Kayıtta faz tamsayı | `phase = 14` | Kayıttan okumak zor |

## İlke: her uçağın kendi paketi

Her uçak **yalnız gerçekte bilebileceğini** içeren kendi paketini alır. Her paketin bir kısmı o uçağın kendi gerçek durumudur. Bu yüzden ground truth'a ihtiyaç olursa C++ düşman uçağın paketinden okuyabilir. Bu, açıkça ve bilerek yapılan bir erişim olur (ör. hata ayıklama, değerlendirme, "her şeyi bilen" deneyler).

```
state
├─ tick, time_s
└─ aircraft
   ├─ blue  ← blue'nun paketi
   │  ├─ own        kendi gerçek durumu (konum, hız, yük, yakıt, sağlık, uygulanan komut, ulaşılabilir g)
   │  ├─ weapons    kendi füzeleri: slot, tip, durum, güdüm kipi, kilit, pitbull, sonuç (veri bağından bilinir)
   │  ├─ contacts   sensörlerinin gördüğü uçaklar (Faz 5'e kadar: gerçek konum, source="truth")
   │  ├─ warnings   RWR / füze uyarısı (Faz 5'e kadar: gerçeğe göre basit kural)
   │  └─ events     bu adımda olanlar (atış kabul/ret, pitbull, kilit kaybı, isabet, kayıp)
   └─ red   ← red'in paketi (aynı yapı)
```

## Paket içeriği (öneri)

### `own`: kendi gerçek durumu

| Alan | Birim | Yeni mi | Not |
|---|---|---|---|
| `id`, `name`, `type`, `team`, `health` | — | hayır | |
| `lat_deg`, `lon_deg`, `alt_m`, `yaw_deg`, `pitch_deg`, `roll_deg`, `tas_mps`, `mach`, `velocity_ned_mps` | SI | hayır | `alt_msl_m` → `alt_m` |
| `load_factor_g` | g | evet | Anlık yük |
| `fuel_kg` | kg | evet | Yakıt kararları için |
| `applied_command` | — | taşındı | Bugünkü `flight_commands`; uçağın kendi paketine |
| `load_limit_reached` | bool | evet | FBW istenen g'ye ulaşamıyor (ör. 10 km'de 6 g istenip ~4 g). C++ dönüş tahminini düzeltebilir |

### `weapons`: kendi füzeleri (veri bağından bilinen)

| Alan | Not |
|---|---|
| `slot`, `profile` (ör. `aim120c5`) | Tip sim'den gelir; C++ loadout tablosu yalnız uyum (fit) seçimi için |
| `state` | `ready` / `in_flight` / `ended` |
| `target_id`, `flight_time_s`, kinematik | |
| `guidance_mode` | `datalink` / `inertial` / `seeker` / `none` |
| `is_locked` | `datalink` ya da `seeker` |
| `is_pitbull` | Bir kez true olunca kalır |
| `outcome` | `hit` / `lost` |

C++'taki `is_pitbull` ve `is_guided` tahminleri kalkar, bu alanlar kullanılır. Tahmin fonksiyonları (`pitbull_time_left_s`, `catch_time_left_s`, WEZ / MAR / DOR) C++'ta kalır; atış kontrol bilgisayarı karşılığı.

### `contacts`: gördüğü uçaklar

Faz 5'e kadar diğer uçakların gerçek konumu, `source: "truth"` ile. Faz 5'te radar gelince `source: "radar"` olur; görünmeyen uçak listeden düşer, ölçüm gürültülü olabilir. C++ bugünkü gibi düşmanın konumunu buradan alır.

### `warnings`: tehditler

Faz 5'e kadar basit kural, gerçek veriden:

| Alan | Kural (Faz 5'e kadar) |
|---|---|
| `missile_launch` | Düşman füzesi bana atıldı (gerçekte RWR / füze uyarı sistemi; burada her zaman görülür) |
| `missile_active` | Bana gelen füze `seeker` kipinde, yani arayıcısı aktif: RWR'nin "missile" uyarısı |
| Füzenin konumu ve hızı | **Seçenek:** gerçek (bugünkü gibi) ya da yalnız yön (gerçekçi RWR). Kullanıcı kararı |

### `events`: bu adımdaki olaylar

`launch_accepted {slot, target}`, `launch_rejected {reason}`, `missile_pitbull`, `missile_lock_lost`, `missile_lock_regained`, `missile_hit`, `missile_lost`, `hit_received`. C++ durum karşılaştırmasıyla olay aramaz; kayıt da bunları aynen yazar.

## Başlangıç el sıkışması (`hello` mesajı)

Simülasyon başında bir kez, `state`'ten önce:
- fizik adımı ve tick süresi;
- uçak listesi (id, tip, takım);
- her uçağın slot → füze profili listesi.

C++ loadout'u buradan kurar. Böylece iki taraftaki tanım uyuşmazlığı ortadan kalkar.

## Komut mesajı

| Değişiklik | Not |
|---|---|
| `launch` emrine `request_id` | Sonraki `state`'te `launch_accepted` / `rejected` aynı id ile döner. Füze eşleştirme tahmini biter |
| Uçuş komutu aynı | `heading_deg`, `altitude_m`, `throttle`, `g_limit`. Hız komutu ve doğrudan mod sonraki aşamada (plan §3.1) |
| `calculations.phase` | Tamsayı yerine ad (`"SUPPORT"`) |

## Birimler

Mesajda yalnız SI ve derece (`_m`, `_mps`, `_deg`, `_s`, `_kg`). NM, kt, ft dönüşümü C++ içinde ve kayıtta. Bugün de büyük ölçüde böyle; v2'de kural olur.

## Kayıt ve görüntüleyici

- Her uçağın paketi kayda girer. Görüntüleyici "blue ne biliyordu" ile "gerçekte ne oldu"yu ayırabilir; bu, sensörler gelince önemli olacak.
- Füze CSV'si: `guidance_mode`, `is_locked`, `is_pitbull`.
- `events.jsonl`: yukarıdaki olaylar.
- Görüntüleyici pitbull ve kilidi füze kaydından okur, C++ hesap dosyasından değil.

## Kesin şema (v2)

Zarf değişmez: satır başına bir JSON, `{"kind", "tick", "data"}`. Python sunucu, C++ istemcidir.

**Akış:** `hello` (Python → C++, bir kez) → [`state` → `command`] × N → `end`.

### `hello`

```json
{"kind": "hello", "tick": 0, "data": {
  "physics_dt_s": 0.008333333, "tick_s": 0.083333333,
  "aircraft": [
    {"id": "blue", "name": "KZL", "type": "F16", "team": "Blue",
     "weapons": [{"slot": 0, "profile": "aim120c5", "designation": "AIM-120C"}, {"slot": 1, "...": "..."}]}
  ]}}
```

- `profile`: sim dosyası (`data/missiles/<profile>.yaml`).
- `designation`: C++'ın `missile_profiles` tablosundaki ad. Tabloda yoksa C++ hata verir.
- Slot sırası `scenario.yaml`'daki `ammo` sırasıdır.

### `state`

```json
{"kind": "state", "tick": 12, "data": {"time_s": 1.0, "aircraft": {"blue": PACKET, "red": PACKET}}}
```

`PACKET`:

```json
{
  "own": {"id": "blue", "name": "KZL", "type": "F16", "team": "Blue", "health": 1,
          "kinematics": KIN, "load_factor_g": 1.0, "fuel_kg": 3100.0,
          "applied_command": {"heading_deg": 0.0, "altitude_m": 10000.0, "throttle": 0.45, "g_limit": null}},
  "weapons": [
    {"slot": 0, "id": "blue/missile/0", "designation": "AIM-120C", "state": "in_flight",
     "request_id": 3, "target_id": "red", "flight_time_s": 12.4, "kinematics": KIN,
     "guidance_mode": "datalink", "is_locked": true, "is_pitbull": false, "outcome": null}
  ],
  "contacts": [{"id": "red", "team": "Red", "source": "truth", "kinematics": KIN}],
  "warnings": [
    {"missile_id": "red/missile/0", "owner_id": "red", "designation": "AIM-120C", "source": "truth",
     "flight_time_s": 20.1, "is_active": false, "kinematics": KIN}
  ],
  "events": [{"type": "launch_accepted", "request_id": 3, "slot": 0, "target_id": "red"}]
}
```

`KIN` (uçak ve füze aynı): `lat_deg, lon_deg, alt_m, yaw_deg, pitch_deg, roll_deg, tas_mps, mach, velocity_ned_mps[3]`. Füzede `roll_deg` = 0.

| Alan | Kural |
|---|---|
| `weapons` | Uçağın bütün slotları, her adım. `ready` slotta `kinematics`, `flight_time_s`, `target_id`, `request_id` null; `guidance_mode` "none". |
| `is_locked` | `guidance_mode` ∈ {datalink, seeker} |
| `is_pitbull` | Bir kez true olunca kalır |
| `contacts` | Kendisi dışındaki bütün uçaklar (Faz 5'e kadar `source: "truth"`) |
| `warnings` | Hedefi bu uçak olan ve havadaki düşman füzeleri. `is_active` = füze `seeker` kipinde |
| `events` | Yalnız bu adımda olanlar; aşağıdaki tablo |

### Olaylar

| `type` | Kimin paketinde | Alanlar |
|---|---|---|
| `launch_accepted` | Atan | `request_id`, `slot`, `target_id` |
| `launch_rejected` | Atan | `request_id`, `reason` (`no_ready_missile` / `invalid_target`) |
| `missile_pitbull` | Atan | `slot` |
| `missile_lock_lost` / `missile_lock_regained` | Atan | `slot`, `guidance_mode` |
| `missile_hit` / `missile_lost` | Atan | `slot`, `target_id` |
| `missile_launch_warning` | Hedef | `missile_id`, `owner_id` |
| `missile_active_warning` | Hedef | `missile_id` |
| `hit_received` | Vurulan | `missile_id`, `health` |

`events.jsonl` aynı olayları `agent` alanıyla yazar. Eski adlar (`missile_launched`, `missile_seeker_lost` / `_acquired`) bu adlarla değişir.

### `command`

```json
{"kind": "command", "tick": 12, "data": {
  "aircraft": [{"id": "blue", "heading_deg": 30.0, "altitude_m": 10000.0, "throttle": 0.5, "g_limit": 3.0}],
  "launch": [{"id": "blue", "target_id": "red"}],
  "calculations": {"aircraft": [{"id": "blue", "phase": "SUPPORT", "decision": "...", "...": "..."}]}}}
```

- `request_id`: isteğe bağlı. C++ göndermez (`{"id", "target_id"}`); sim ilk hazır füzeyi atar, C++ atılan slotu sonraki `state`'in `weapons`'ından görür. `launch_accepted` / `rejected` olaylarında `null` döner ve C++ bunları okumaz.
- Bir adımda bir uçak için birden çok atış emri olabilir; her biri sırayla, kendi `request_id`'siyle.

### v2.1 eklemeleri: sensörler ve doğrudan mod (karar 2026-11-12)

> **Sensörler varsayılan KAPALI** (kullanıcı kararı: algoritmaya odaklanmak için). `scenario.yaml` `world.sensors: false`: `contacts` hep gerçek (`source: "truth"`, `tracked: true`), `rwr` boş, veri bağı yalnız ±60° açı kuralı. `true` yapılınca aşağıdaki radar kuralları çalışır.

**Radar (sim, `sensors/radar.py`).** Kaynak: OPRF F-16 `Nasal/radar/apg-68.nas`, `radar/radar-system.nas`, `radar/rcs_mand.nas`.

| Kural | Değer | Kaynak |
|---|---|---|
| Tespit menzili | R = 70 NM · (σ / 3.2 m²)^(1/4) | apg-68.nas `rcsRefDistance`, `rcsRefValue`, satır ~530 |
| Hedef RCS | F-16: 2 m² (ön görünüş, açıdan bağımsız kabul) | rcs_mand.nas |
| Görüş alanı | Burna göre azimut ±60°, yükseliş ±60° (antende yatış dengelenmiş kabul) | apg-68.nas field of regard |
| Notch | Hedef benden alçaktaysa (aşağı bakış, düz yer) ve hedefin görüş hattı boyunca hızı \|v·LOS\| < 30 kt ise görünmez | radar-system.nas `checkClutter`, `doppler_speed_kt` |
| Çok yollu yayılma, arazi, tarama süresi, RCS'in açıya bağlılığı | Yok (sadeleştirme) | — |

**`contacts` (değişti).** Bir uçak hiç görülmediyse listede yok. Görüldüyse kalır:

```json
{"id": "red", "team": "Red", "source": "radar", "tracked": true, "age_s": 0.0, "kinematics": KIN}
```

`tracked` false iken `kinematics` son ölçümdür ve `age_s` son ölçümden bu yana geçen süredir. Sim tahmin yürütmez.

**`rwr` (yeni, `own` yanında):** beni gören düşman radarları.

```json
"rwr": [{"emitter_id": "red", "bearing_deg": 12.0}]
```

**`warnings`:** şimdilik gerçek veri (kullanıcı kararı A). Füze RCS'i için kaynak bulunursa radara bağlanır.

**Veri bağı:** füze, atan uçağın radarı hedefi **görüyorsa** (menzil + görüş alanı + notch) güncel bilgi alır. Yalnız açı yetmez.

**Doğrudan mod (komut).** `aircraft[]` öğesinde isteğe bağlı `mode`:

| `mode` | Alanlar | Not |
|---|---|---|
| `"target"` (varsayılan, alan yoksa) | `heading_deg`, `altitude_m`, `throttle`, `g_limit?` | Bugünkü |
| `"direct"` | `bank_deg` (−180..180), `g` (−4..9), `throttle` | f16.xml FBW: +9 / −4 g. AML'nin yatış hızı ve g döngüleri doğrudan bu hedeflerle çalışır |

C++ tarafında yalnız altyapı: `FlightCommand` doğrudan modu taşıyabilir, yazıcı yazar. Hiçbir manevra henüz kullanmaz.

### `end`

Değişmedi: `{"kind": "end", "tick": N, "data": {}}`.

## Uygulama sırası (öneri)

1. Protokol belgesi (bu dosya) kesinleşir; Python ve C++ için ortak örnek mesajlar (`docs/mimari/mesaj_sablonlari/`).
2. Sim tarafı: paket üretimi, el sıkışma, olaylar, kayıt.
3. C++ tarafı: okuyucu ve yazıcı, eşleştirme, tahminlerin sim değeriyle değiştirilmesi.
4. Görüntüleyici.
5. Uçtan uca test: aynı senaryo eski ve yeni protokolle karar akışı karşılaştırması.
