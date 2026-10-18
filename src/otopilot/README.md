# Otopilot (C++)

Python simülasyonuna TCP ile bağlanan, fighter'ı (blue) süren C++ programı.
Her adımda ölçümü alır, nesnelere okur, hesaplar, bir manevra uygular ve komutu geri gönderir.

Kod, **fighter'ın içinde çalışıyormuş gibi** düşünülür: kendi uçağımız `fighter`,
saptadığımız düşman `bogey`. Yalnız fighter sürülür; bogey'e her adım YAML'daki
başlangıç komutu aynen geri gönderilir (hold).

Büyük resim, diyagramlar ve alınan kararlar: [docs/mimari/OTOPILOT_MIMARISI.md](../../docs/mimari/OTOPILOT_MIMARISI.md).
Bu dosya kodun kendisini, dosya dosya anlatır.

---

## İçindekiler

1. [Derle ve çalıştır](#1-derle-ve-çalıştır)
2. [Bir step'te ne oluyor](#2-bir-stepte-ne-oluyor)
3. [Dosyalar tek tek](#3-dosyalar-tek-tek)
4. [Veri: uçağın içi, birimler ve kaynaklar](#4-veri-uçağın-içi-birimler-ve-kaynaklar)
5. [Kodda uyduğumuz kurallar](#5-kodda-uyduğumuz-kurallar)
6. [Nasıl yapılır](#6-nasıl-yapılır)
7. [Henüz boş olanlar ve sınırlar](#7-henüz-boş-olanlar-ve-sınırlar)

---

## 1. Derle ve çalıştır

Sıfırdan kurulum (Python ortamı, BVRGym, w64devkit), görüntüleyici, testler ve sorun giderme:
[docs/CALISTIRMA_KILAVUZU.md](../../docs/CALISTIRMA_KILAVUZU.md). Aşağısı kısa özet.

Derleme CMake ile yapılır. Derleyici, CMake ve make w64devkit'ten gelir
(`%LOCALAPPDATA%\Programs\w64devkit\bin`, PATH'te).

**Bir kez (yapılandırma):** `build/` klasörünü ve Makefile'ı oluşturur.

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles"
```

**Her derlemede:**

```powershell
cmake --build src/otopilot/build
```

Çıktı `src/otopilot/main.exe`. Yalnız değişen dosyalar yeniden derlenir. Klasöre yeni
bir `.cpp` eklemek için CMake'i yeniden çalıştırmak gerekmez (`CONFIGURE_DEPENDS`).
`CMakeLists.txt` değişirse ya da `build/` silinirse yapılandırmayı tekrarla.
`build/` ve `main.exe` `.gitignore`'dadır.

> Derlemeden önce bütün dosyaları kaydet (`Ctrl+K S`). Kaydedilmemiş dosyayı derleyici
> görmez; tipik belirtisi `'X' has not been declared` hatasıdır.

**Çalıştırma (iki terminal):**

1. `python .\src\sim\sim_main.py`: Python sunucusu açılır, bağlantı bekler.
2. `.\src\otopilot\main.exe`: C++ bağlanır ve döngü başlar.

Python'u önce başlat. Varsayılan adres `127.0.0.1:6000`; port Python'da
`src/sim/scenario.yaml` → `connection.port`'tan gelir. Değiştirirsen C++'a da ver:
`.\src\otopilot\main.exe 127.0.0.1 6001`.

Süre dolunca Python `end` gönderir; C++ `Simulasyon bitti. Son tick: ...` yazıp kapanır.
Python tarafı `Bitti. N snapshot, M olay.` yazar ve kaydı `recordings/` altına bırakır.

**Terminal çıktısı:** saniyede bir satır, iki uçak yan yana:

```
t  12 s | blue  lat 59.1990  lon 18.4290  alt 5004 m  hdg 26.1  tas 305 m/s  rng 88.4 km  brg -19.3  msl 4 | red ...
```

`rng` ve `brg` fighter için bogey'e göre, bogey için fighter'a göredir.
`msl` hazır füze sayısı; havada füze varsa `+havada` eklenir.

---

## 2. Bir step'te ne oluyor

Python bir ölçüm (`state`) gönderir ve C++ cevap verene kadar fiziği **durdurur**.
`Autopilot::step` cevabı şu beş adımda üretir:

```
1. Ölçüm   StateReader::read_world           JSON → world_ (fighter, bogey, saat, füzeler, profil)
           StateReader::read_flight_command  JSON → fighter_command_, bogey_command_
2. Hesap   world_.update()                   own (füze sayıları), enemy (Geometry + Weapons)
3. Manevra maneuvers_.hold.compute(...)      → fighter_command_      (elle seçilir)
4. Atış    bool fighter_fire                 → true ise blue, red'e atar (elle, tek seferlik)
5. Komut   CommandWriter::write              → command JSON (iki uçuş komutu + launch)
```

Sonra Python atışları uygular (`fire`), uçakları bir adım ilerletir (`step`), kaydeder
ve yeni ölçümü gönderir. Bir step yaklaşık 1/12 simülasyon saniyesidir.

Diyagram: [05_otopilot_step.svg](../../docs/mimari/assets/05_otopilot_step.svg).

**Sıra önemli:**
- `read_flight_command`, manevradan **önce** çağrılır. Manevra mevcut komutu `current`
  olarak alıp yalnız ilgilendiği alanı değiştirir. Önce okunmazsa irtifa ve gaz 0 giderdi.
- `world_.update()`, `read_world`'den **sonra** çağrılır. Hesap, o adımın ölçümüyle yapılır.

**Tick 0'a özel bir şey yok.** Python her adım son uygulanan komutları `flight_commands`
içinde geri gönderir:
- fighter için bu, bir önceki adımda bizim gönderdiğimiz komuttur (ilk adımda YAML başlangıcı),
- bogey için hiç değiştirmediğimizden hep YAML başlangıcıdır.

---

## 3. Dosyalar tek tek

Katmanlar: **okuma** (StateReader) → **durum** (World, Aircraft) → **hesap** (World::update,
Geometry, Weapons) → **uçuş** (Maneuvers) → **yazma** (CommandWriter). TCP ayrı ve dokunulmaz.
Diyagram: [04_otopilot_katmanlar.svg](../../docs/mimari/assets/04_otopilot_katmanlar.svg).

| Dosya | Katman | Bir cümlede |
|---|---|---|
| `main.cpp` | giriş | Bağlan, döngüde `step` çağır, hatada dur. |
| `autopilot.hpp/.cpp` | uygulama | Bir step'in beş adımı; nesneleri tutar. |
| `state_reader.hpp/.cpp` | okuma | JSON → var olan nesnelerin içi. |
| `world.hpp/.cpp` | durum + hesap | fighter, bogey, saat; `update()` hesapları doldurur. |
| `aircraft.hpp` | durum | `Kinematics`, `Missile`, `OwnInfo`, `EnemyInfo`, `Aircraft`. |
| `aircraft_profiles.hpp` | durum | Uçak tipi → sabit özellikler tablosu. |
| `geometry.hpp/.cpp` | hesap | İki konum arası: range, bearing. |
| `weapons.hpp/.cpp` | hesap | İki uçak arası: WEZ, MAR, DOR. |
| `maneuver.hpp/.cpp` | uçuş | `Maneuver` arayüzü, `Hold`, `Crank`, `Maneuvers` kutusu. |
| `orders.hpp` | emir | `FlightCommand`: yön, irtifa, gaz. |
| `command_writer.hpp/.cpp` | yazma | Emir → command JSON, sınır kontrolü. |
| `roles.hpp` | ortak | `FIGHTER_ID = "blue"`, `BOGEY_ID = "red"`. |
| `angles.hpp` | ortak | `PI`, derece/radyan, `wrap_180`, `wrap_360`. |
| `connection.hpp`, `tcp_connection.cpp`, `messages.hpp` | TCP | Dokunulmaz: bağlantı ve mesaj çerçevesi. |
| `third_party/nlohmann` | dış | JSON kütüphanesi 3.12.0 (MIT). |
| `CMakeLists.txt` | derleme | Klasördeki bütün `.cpp`'ler → `main.exe`. |

### `main.cpp`

Programın girişi. Üç nesne kurar ve döngüyü çevirir:

```cpp
Connection connection(argc, argv);   // Python'a bağlan
Autopilot autopilot;
while (connection.receive(state))    // Python "end" gönderince false
{
    Message command = autopilot.step(state);
    connection.send(command);
}
```

TCP ayrıntısı da uçuş mantığı da burada değildir. Herhangi bir yerde `throw` edilen
hata buraya düşer, `Hata: ...` olarak yazılır ve program 1 ile çıkar.

### `autopilot.hpp/.cpp`: `Autopilot`

Bir step'i yürüten sınıf. [2. bölümdeki](#2-bir-stepte-ne-oluyor) beş adım `step()` içindedir.

**Tuttukları (üye değişkenler):** `Autopilot` ile bir kez oluşur, program boyunca yaşar,
her step yalnız içleri dolar.

| Üye | Ne |
|---|---|
| `world_` | `World`: fighter, bogey, saat. |
| `fighter_command_` | Fighter'a giden komut; manevranın sonucu. |
| `bogey_command_` | Bogey'e giden komut; YAML başlangıcı, aynen geri gider (hold). |
| `maneuvers_` | Bütün manevralar (`Maneuvers`). Her biri kendi durumunu korur. |
| `last_print_s_` | Terminale en son yazılan saniye; saniyede bir satır için. |

**Manevrayı ve atışı elle seçmek** `step()` içinde yapılır:

```cpp
// 3. Manevra: denemek istediğini aç, diğerini kapat.
fighter_command_ = maneuvers_.hold.compute(world_.fighter, fighter_command_);
// fighter_command_ = maneuvers_.crank.compute(world_.fighter, fighter_command_);

// 4. Atış: tek seferlik.
bool fighter_fire = false;
// bool fighter_fire = world_.sim_time_s > 10.0 && world_.sim_time_s < 11.0;
```

`fighter_fire` neden üye değil de yerel? Uçuş komutu kalıcıdır, bir sonraki step'e taşınır.
Atış ise o step'e aittir; yerel değişken step bitince kaybolur, "gönderdikten sonra temizle"
gerekmez.

`print_aircraft` dosya içinde `static` bir yardımcıdır (yalnız bu `.cpp`'de görünür),
terminale tek satırlık durum yazar.

### `state_reader.hpp/.cpp`: `StateReader`

**JSON'dan C++ nesnesine çeviren tek yer.** Nesne oluşturmaz: verilen nesnenin içini doldurur.
Hiçbir şey saklamadığı için bütün fonksiyonları `static`'tir; `StateReader::read_world(...)`
diye nesnesiz çağrılır.

| Fonksiyon | Erişim | Ne okur | Nereye yazar |
|---|---|---|---|
| `read_world(data, world)` | public | `sim_time_s`, `aircraft[]`, `missiles[]` | `world` |
| `read_flight_command(data, id, command)` | public | `flight_commands[]` içinde `id`'li olan | `command` |
| `read_aircraft(j, aircraft)` | private | kimlik, sağlık; `type` ile profil | `aircraft` |
| `read_kinematics(j, k)` | private | konum, yönelim, hız | `k` |
| `read_missiles(missiles, owner_id, out)` | private | sahibi `owner_id` olan füzeler | `out` listesi |
| `read_missile(j, missile)` | private | tek füze | `missile` |

Çağrı zinciri:

```
read_world
 ├─ read_aircraft(blue) → profil tablosu, read_kinematics
 ├─ read_aircraft(red)  → profil tablosu, read_kinematics
 ├─ read_missiles(..., "blue", fighter.missiles) → her füze için read_missile → (atıldıysa) read_kinematics
 └─ read_missiles(..., "red",  bogey.missiles)   → ...
```

**Ayrıntılar:**
- `read_missiles` (çoğul) listeyle, `read_missile` (tekil) tek füzeyle ilgilenir; iki ayrı fonksiyondur.
- Füze listesi her step `clear()` edilip baştan yazılır. Füze sayısı uçuş boyunca sabit
  olduğu için pratikte aynı füzeler aynı sırayla yeniden dolar.
- Atılmamış füzede `target_id`, `outcome`, `flight_time_s`, `kinematics` JSON'da `null` gelir.
  `null` string'e ya da sayıya çevrilemez; önce `is_null()` ile bakılır. Sayı ve konum
  `std::optional` içinde boş bırakılır, metinler boş string olur.
- Yön burada çevrilir: JSON'daki 0..360 `yaw_deg` / `heading_deg` → `wrap_180` → ±180.
- `fighter` ya da `bogey` JSON'da yoksa, ya da `flight_commands`'ta aranan id yoksa
  **hata verip durur**. Sessizce 0 bırakmak, otonominin 0,0 koordinatına göre karar
  vermesi demek olurdu.
- Profil JSON'dan değil, `type` ile `aircraft_profiles.hpp` tablosundan alınır. Bilinmeyen tipte durur.
- Okurken hep `.at("alan")` kullanılır. `[]` olmayan alanı sessizce oluştururdu; `.at` hata verir.

### `world.hpp/.cpp`: `World`

1v1 dünya: `sim_time_s`, `fighter`, `bogey`. Alanları public `struct`'tır; herkes okuyabilir.

`update()` ölçüm okunduktan sonra her step bir kez çağrılır ve iki uçağın **hesaplanan**
bölümlerini doldurur:

```cpp
void World::update()
{
    compute_own_info(fighter, fighter.own);
    compute_own_info(bogey, bogey.own);

    fill_enemy_info(fighter, bogey);   // bizim bogey'e bakışımız
    fill_enemy_info(bogey, fighter);   // bogey'in bize bakışı
}
```

- `compute_own_info`: füze listesinden hazır füze sayısını ve havadaki füzeyi sayar.
  Nesne step'ler arasında yaşadığı için sayaçlar **önce sıfırlanır**; yoksa her step
  bir öncekinin üstüne eklenirdi.
- `fill_enemy_info`: önce `Geometry` (range, bearing), sonra `Weapons` (in_wez, MAR, DOR).
  Sıra bilinçlidir: `Weapons`, az önce dolan `own.enemy.range_m`'yi okuyabilir.
- Aynı fonksiyon iki uçak için roller yer değiştirerek çağrılır. fighter ve bogey'in aynı
  sınıftan olmasının faydası budur.

İki yardımcı da dosya içinde `static`'tir; dışarıdan çağrılmaz.

### `aircraft.hpp`: uçağın veri yapıları

JSON bilmez. Beş `struct` vardır; ayrıntılı alan tabloları [4. bölümde](#4-veri-uçağın-içi-birimler-ve-kaynaklar).

| Yapı | Ne | Kim doldurur |
|---|---|---|
| `Kinematics` | Konum, yönelim, hız. Uçakta ve atılmış füzede aynı yapı. | `StateReader` |
| `Missile` | Uçağın bir füzesi; sahibi onu listesinde tutan uçaktır. | `StateReader` |
| `OwnInfo` | Kendi bilgilerim: hazır / havadaki füze. | `World::update` |
| `EnemyInfo` | Karşı uçağa göre: range, bearing, in_wez, MAR, DOR. | `World::update` |
| `Aircraft` | Hepsini taşıyan uçak. fighter ve bogey aynı sınıf. | ikisi birlikte |

Her alanın yorumu üç sütundur: `birim | JSON anahtarı | Python / JSBSim kaynağı`.
Bir değerin nereden geldiğini merak ettiğinde bu dosyaya bakman yeter.

`std::optional<T>`: "değer var ya da yok". Atılmamış füzenin konumu yoktur; 0 yazsaydık
0,0 koordinatı sanılırdı. Kullanımı: `if (m.kinematics) { m.kinematics->lat_deg; }`.

### `aircraft_profiles.hpp`: uçak tipi tablosu

Simülasyondan gelmeyen, elle girilen sabitler: Mach, g, WEZ sınırları, radar menzili ve
gimbal yarı açısı. Anahtar `scenario.yaml`'daki `type`'tır (`F16`, `Su57`, `CustomAircraft`).

- `aircraft_profiles()`: tabloyu döndürür. İçindeki `static const` tablo ilk çağrıda bir kez kurulur.
- `aircraft_profile(type)`: tipin profilini döndürür; tabloda yoksa
  `Bilinmeyen ucak tipi: ... (aircraft_profiles.hpp'ye ekle)` ile durur.

Değerlerin kaynağı dosyanın içinde yazılıdır. F16'nın WEZ değeri gerçek AIM-120 değil,
BVRGym füze modelinde ölçülmüştür; Su57 ve CustomAircraft satırları örnektir. Fizik modeli
her tipte F-16'dır; `type` yalnız C++ hesaplarında kullanılan bir etikettir.

`radar_gimbal_half_deg` **yarım** açıdır: 60, burundan sağa ve sola 60° (±60) demektir.
Crank'ta tekrar ikiye bölme.

### `geometry.hpp/.cpp`: `Geometry`

**Saf geometri.** İki `Kinematics` alır, hiçbir şey saklamaz, fonksiyonları `static`'tir.

| Fonksiyon | Döndürür |
|---|---|
| `range_m(own, target)` | m, 3B düz çizgi mesafe |
| `bearing_deg_180(own, target)` | derece ±180, own'dan target'a bakış yönü; 0 kuzey, + doğu |

`own` bakan taraftır, hesap onun yerinden yapılır. Neden `Aircraft` değil `Kinematics`?
Mesafe için yalnız konum gerekir; aynı fonksiyon ileride füze–uçak arasında da çalışır
(`Geometry::range_m(own.kinematics, *missile.kinematics)`).

ECEF, LOS gibi ara yapılar gerekirse bu dosyalara, dışarıya kapalı olacaksa `private static` olarak eklenir.

### `weapons.hpp/.cpp`: `Weapons`

**Füze ve taktik hesapları.** `Geometry`'den farkı: yalnız konuma değil, uçakların
profiline de (füze menzili, radar) bakar. Bu yüzden `Aircraft` alır.

| Fonksiyon | Döndürür |
|---|---|
| `in_wez(own, enemy)` | Karşı uçak benim füzemin etkin menzil aralığında mı |
| `mar_m(own, enemy)` | m, Minimum Abort Range |
| `dor_m(own, enemy)` | m, Desired Out Range |

Çağrıldığında `own.enemy.range_m` gibi `Geometry` sonuçları zaten doludur.

### `maneuver.hpp/.cpp`: manevralar

```cpp
class Maneuver                     // ortak arayüz: "bu durumda nasıl uçarım"
{
public:
    virtual ~Maneuver() = default;
    virtual FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) = 0;
};

class Hold  : public Maneuver { ... };   // mevcut komutu aynen koru
class Crank : public Maneuver { ... };   // hedefi gimbal sınırına yakın tutarak yanal kaçış

struct Maneuvers { Hold hold; Crank crank; };   // hepsi bir arada
```

- **`compute(fighter, current)`:** `fighter` ölçümü (`kinematics`) ve hesaplananları
  (`own`, `enemy`) taşır. `current` mevcut komuttur; manevra onu kopyalayıp yalnız ilgilendiği
  alanı değiştirir (crank yalnız yönü).
- **`virtual ... = 0`:** saf sanal; `Maneuver` tek başına oluşturulamaz, her manevra bu
  fonksiyonu yazmak zorundadır.
- **`override`:** "üst sınıftakinin yerine geçiyor". İmza yanlışsa derleyici hata verir.
- **`compute` neden `const` değil?** Manevra kendi durumunu güncelleyebilsin diye
  (ör. crank'a ilk girişte tarafı seçip hatırlamak).
- **`Maneuvers`:** manevraları bir arada tutan kutu. Her manevra bir kez oluşur ve kendi
  durumunu korur. Yeni manevra eklemek `autopilot.hpp`'ye dokunmayı gerektirmez.
  İleride karar mekanizması geldiğinde ona tek bir `Maneuvers&` verilir.

Hangi manevranın çalışacağı şimdilik `Autopilot::step` içinde **elle** seçilir.

### `orders.hpp`: `FlightCommand`

Uçağa giden kalıcı uçuş hedefi: `heading_deg_180` (±180), `altitude_m`, `throttle` (0..1).
JSON bilmez, `id` taşımaz; hangi uçağın komutu olduğunu tutan değişkenin adı söyler.

### `command_writer.hpp/.cpp`: `CommandWriter`

**C++ nesnesinden JSON'a çeviren tek yer.** `static`.

```cpp
Message write(tick, fighter, bogey, fighter_fire);
```

Ürettiği mesaj:

```json
{"kind": "command", "tick": 24, "data": {
  "aircraft": [{"id": "blue", "heading_deg": 270, "altitude_m": 5000, "throttle": 0.55},
               {"id": "red",  "heading_deg": 139, "altitude_m": 5000, "throttle": 0.55}],
  "launch":   [{"id": "blue", "target_id": "red"}]
}}
```

- `heading_deg_180` → `wrap_360` → 0..360 (Python bunu bekler).
- `launch` yalnız `fighter_fire` doğruysa dolar; 1v1'de hedef hep bogey.
- **Sınır kontrolü:** NaN/sonsuz değerde ya da 0..1 dışındaki throttle'da, uçak adı ve
  değerle durur (`blue: throttle 0..1 disinda: 1.200000`). Python aynı durumda durur ama
  hangi hesabın bozulduğunu söyleyemez. Heading kontrol edilmez; `wrap_360` her değeri düzeltir.
- `to_json` (tek uçağın komutu) `private`'tır; dışarıdan yalnız `write` görünür.

### `roles.hpp`

```cpp
inline constexpr const char* FIGHTER_ID = "blue";
inline constexpr const char* BOGEY_ID = "red";
```

"blue = fighter, red = bogey" eşlemesi yalnız burada. YAML'da anahtar değişirse yalnız burası değişir.

### `angles.hpp`

- `PI`, `DEG_TO_RAD`, `RAD_TO_DEG` (MinGW c++17'de `M_PI` yok).
- `wrap_180(deg)`: herhangi bir açı → (-180, 180]. Okunan yönde de, hesaptaki açı farklarında
  da (ör. `bearing - heading`) bunu kullan. **±180'e çeviren tek fonksiyon budur.**
- `wrap_360(deg)`: herhangi bir açı → [0, 360). Yalnız Python'a yön gönderirken.

Fonksiyonlar `inline`'dır: başlıkta gövdesi olan ve birden çok `.cpp`'nin include ettiği
fonksiyon `inline` olmazsa linker "multiple definition" verir.

### TCP: `connection.hpp`, `tcp_connection.cpp`, `messages.hpp` (dokunulmaz)

- `Message`: `kind` (`state` / `command` / `end`), `tick`, `data` (JSON).
- `Connection::receive(state)`: bir ölçüm al; Python `end` gönderdiyse `false`.
- `Connection::send(command)`: komutu gönder; komut son alınan ölçümle aynı `tick`'te olmalı.

Telde her mesaj tek satır UTF-8 JSON + `\n`, en fazla 1 MiB. Parçalı ve birleşik gelen
satırlar doğru ayrılır. Adım sırası ve mesaj türü kontrol edilir. Zaman aşımı ve otomatik
yeniden bağlanma yoktur.

---

## 4. Veri: uçağın içi, birimler ve kaynaklar

Diyagram: [06_aircraft_veri.svg](../../docs/mimari/assets/06_aircraft_veri.svg).

Zincir: JSBSim property → Python `src/sim/simulation.py` → state JSON → `StateReader` → `Aircraft`.

### Kimlik (`Aircraft`), JSON `data.aircraft[i]`

| Alan | Birim | JSON | Kaynak |
|---|---|---|---|
| `id` | metin, `"blue"` / `"red"` | `id` | `scenario.yaml` agents anahtarı |
| `name` | metin | `name` | `scenario.yaml` name |
| `team` | metin, `"Blue"` / `"Red"` | `team` | `scenario.yaml` team |
| `type` | metin | `type` | `scenario.yaml` type |
| `health` | 0..1, 1 sağlam | `health` | BVRGym `agent.healthPoints` |

### `Kinematics`, uçakta en üst seviyede, füzede `kinematics` altında

| Alan | Birim | JSON | JSBSim property | C++'ta çeviri |
|---|---|---|---|---|
| `lat_deg` | derece, WGS84 jeodezik | `lat_deg` | `position/lat-geod-deg` | yok |
| `lon_deg` | derece | `lon_deg` | `position/long-gc-deg` | yok |
| `alt_msl_m` | m, deniz seviyesinden | `alt_msl_m` | `position/h-sl-meters` | yok |
| `heading_deg_180` | derece ±180; 0 kuzey, + doğu | `yaw_deg` (0..360) | `attitude/psi-deg` | `wrap_180` |
| `pitch_deg` | derece; + burun yukarı | `pitch_deg` | `attitude/theta-deg` | yok |
| `roll_deg` | derece; + sağ kanat aşağı | `roll_deg` | `attitude/phi-deg` | yok |
| `tas_mps` | m/s, gerçek hava hızı | `tas_mps` | `velocities/vt-fps` × 0,3048 | yok |
| `mach` | birimsiz | `mach` | `velocities/mach` | yok |
| `velocity_ned_mps` | m/s, [kuzey, doğu, aşağı] | `velocity_ned_mps` | `velocities/v-north-fps`, `v-east-fps`, `v-down-fps` × 0,3048 | yok |

Not: BVRGym spawn enlemini jeosantrik kabul eder; Python jeodezik enlemi okur. Bu yüzden
YAML'da `lat: 59.0` yazan uçak C++'ta yaklaşık `59.17` görünür. Hata değildir.

### `Missile`, JSON `data.missiles[i]`, `owner_id` ile sahibinin listesine

| Alan | Birim | JSON | Not |
|---|---|---|---|
| `id` | metin, `"blue/missile/0"` | `id` | |
| `target_id` | metin; atılmadıysa boş | `target_id` (null olabilir) | |
| `state` | `"ready"` / `"in_flight"` / `"ended"` | `state` | BVRGym `is_ready_to_launch()`, `is_active()` |
| `outcome` | `"hit"` / `"lost"`; yoksa boş | `outcome` (null olabilir) | |
| `flight_time_s` | s, atıştan beri; `optional` | `flight_time_s` (null olabilir) | |
| `kinematics` | `Kinematics`; `optional` | `kinematics` (null olabilir) | atılınca uçakla aynı alanlar |

### `AircraftProfile`, JSON'dan değil, `type` ile tablodan

| Alan | Birim | F16 |
|---|---|---|
| `max_mach` | birimsiz | 2.05 |
| `max_g` | g | 9 |
| `wez_max_range_m` | m, füze etkin menzil dış sınırı | 31 000 |
| `wez_min_range_m` | m, iç sınır | 2 000 |
| `radar_range_m` | m, algılama menzili | 80 000 |
| `radar_gimbal_half_deg` | derece, yarım açı (60 = ±60) | 60 |

### `OwnInfo`, hesaplanan: kendi bilgilerim

| Alan | Birim | Nasıl |
|---|---|---|
| `ready_missiles` | adet | `missiles` içinde `state == "ready"` olanlar |
| `missile_in_flight` | adet, havadaki kendi füzem | `state == "in_flight"` olanlar. Python aynı anda en çok bir füzeye izin verir. |

İhtiyaç oldukça alan eklenir (ör. irtifa/hıza bağlı dinamik WEZ).

### `EnemyInfo`, hesaplanan: karşı uçağa göre

`fighter.enemy`: bizim bogey'e bakışımız. `bogey.enemy`: bogey'in bize bakışı
(ör. `bogey.enemy.in_wez` = ben onun WEZ'inde miyim).

| Alan | Birim | Fonksiyon |
|---|---|---|
| `range_m` | m, 3B mesafe | `Geometry::range_m` |
| `bearing_deg_180` | derece ±180, kuzeye göre karşı uçağın yönü | `Geometry::bearing_deg_180` |
| `in_wez` | evet/hayır | `Weapons::in_wez` |
| `mar_m` | m, Minimum Abort Range | `Weapons::mar_m` |
| `dor_m` | m, Desired Out Range | `Weapons::dor_m` |

### `FlightCommand`, giden emir

| Alan | Birim | JSON | Çeviri |
|---|---|---|---|
| `heading_deg_180` | derece ±180 | `heading_deg` (0..360) | okurken `wrap_180`, yazarken `wrap_360` |
| `altitude_m` | m, MSL | `altitude_m` | yok |
| `throttle` | 0..1 | `throttle` | yok; aralık dışında durur |

---

## 5. Kodda uyduğumuz kurallar

| Kural | Neden |
|---|---|
| **JSON'u yalnız `StateReader` ve `CommandWriter` bilir.** | Hesap ve manevra kodu yalnız C++ nesneleri görür; JSON alan adı değişirse iki dosya değişir. |
| **Nesneler bir kez oluşur, her step içleri dolar.** | Step'ler arası hafıza (manevra durumu) mümkün olur. Sayaçlar her step sıfırlanmalıdır. |
| **Yön C++ içinde ±180, adında `_180`.** | Hesap tek bir aralıkta yapılır. Çeviri yalnız okurken (`wrap_180`) ve yazarken (`wrap_360`). |
| **Birim alan adında:** `_m`, `_deg`, `_s`, `_mps`. | Okurken birim düşünülmez. |
| **Her alanın yorumunda birim ve kaynak.** | Bir değerin nereden geldiği dosyadan okunur. |
| **Hiçbir şey saklamayan sınıfın fonksiyonları `static`.** | `StateReader`, `CommandWriter`, `Geometry`, `Weapons`: nesnesiz çağrılır; çağrılar birbirini etkilemez. |
| **Ad öneki işi söyler:** `read_` JSON okur, `compute_` / `fill_` hesaplar, `compute` manevra. | Fonksiyonun hangi katmanda olduğu adından anlaşılır. |
| **Bulunamayan veri sessizce 0 kalmaz, hata verir.** | 0 değerleriyle uçmak, durmaktan daha tehlikelidir. |
| **Hata mesajları ASCII.** | Windows konsolunda Türkçe karakter bozulabilir. |
| **Üye değişkenler `_` ile biter** (`world_`). Public `struct` alanları bitmez (`world.fighter`). | Yerel değişkenle sınıf üyesi okurken ayırt edilir. |
| **Dosya içi yardımcılar `static` serbest fonksiyon.** | `print_aircraft`, `compute_own_info`: yalnız o `.cpp`'de görünür. |

`static` kelimesi C++'ta üç anlamda geçer, bu kodda üçü de var:
1. **Sınıfta** (`static void read_world`): nesnesiz çağrılan fonksiyon.
2. **Dosyada** (`static void print_aircraft`): yalnız bu `.cpp`'de görünür.
3. **Fonksiyon içinde** (`static const ... table`): ilk çağrıda bir kez oluşur, program boyu yaşar.

---

## 6. Nasıl yapılır

### Yeni manevra ekle

1. `maneuver.hpp`: sınıfı yaz.
   ```cpp
   // Düşmana doğru dön.
   class Pursuit : public Maneuver
   {
   public:
       FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;
   };
   ```
2. `maneuver.cpp`: gövdesini yaz.
   ```cpp
   FlightCommand Pursuit::compute(const Aircraft& fighter, const FlightCommand& current)
   {
       FlightCommand command = current;                            // irtifa ve gaz aynı
       command.heading_deg_180 = fighter.enemy.bearing_deg_180;    // burnu düşmana çevir
       return command;
   }
   ```
3. `maneuver.hpp` → `Maneuvers`: `Pursuit pursuit;` satırını ekle.
4. `autopilot.cpp` → `step`: `fighter_command_ = maneuvers_.pursuit.compute(world_.fighter, fighter_command_);`

Manevranın kendi ayarları ve durumu (ör. crank tarafı) sınıfın `private` üyeleri olarak durur.

### Elle atış

`autopilot.cpp` → `step` → 4. madde. Örnek: 10. saniyede tek atış.

```cpp
bool fighter_fire = world_.sim_time_s > 10.0 && world_.sim_time_s < 11.0;
```

O bir saniyede emir 12 kez gitse de Python havada kendi füzesi varken yenisini atmaz.
Sonucu terminalde `msl 3 +havada`, kayıtta `events.jsonl` ve görüntüleyicide **Füze vuruş bilgisi** gösterir.

### Hesaplanan yeni bilgi ekle (ör. ATA)

1. `aircraft.hpp` → `EnemyInfo`: `double ata_deg = 0;   // derece, ... | Geometry::ata_deg`
2. `geometry.hpp/.cpp`: `static double ata_deg(const Kinematics& own, const Kinematics& target);`
3. `world.cpp` → `fill_enemy_info`: `own.enemy.ata_deg = Geometry::ata_deg(own.kinematics, target.kinematics);`
4. İstersen `autopilot.cpp` → `print_aircraft`'a ekle.

Profil ya da füze bilgisine ihtiyaç duyuyorsa (2. adım) `Geometry` yerine `Weapons`'a yaz.
Kendi bilgimse (ör. dinamik WEZ) `OwnInfo` ve `compute_own_info`.

### Yeni uçak tipi

`aircraft_profiles.hpp` tablosuna bir satır, `scenario.yaml`'da `type: YeniTip`.
Tablodaki sıra `AircraftProfile`'daki alan sırasıyla aynıdır.

### Profile yeni özellik

`AircraftProfile`'a alanı ekle, tablodaki her satıra değerini aynı sırada yaz.
Kullanım: `world_.fighter.profile.yeni_alan`.

### Python'dan gelen yeni ölçüm alanı

Python `simulation.py` alanı JSON'a koyduktan sonra: `aircraft.hpp`'de alanı (birim ve
kaynak yorumuyla), `state_reader.cpp`'de ilgili `read_` fonksiyonuna bir satır.

---

## 7. Henüz boş olanlar ve sınırlar

**İçi boş (TODO), iskelet hazır:**
- `Geometry::range_m`, `Geometry::bearing_deg_180`: şimdilik 0 döndürür.
- `Weapons::in_wez`, `mar_m`, `dor_m`: şimdilik `false` / 0.
- `Crank::compute`: şimdilik komutu değiştirmez.

Gövdeler `(void)own;` gibi satırlarla "kullanılmayan parametre" uyarısını susturur;
hesap yazılınca bu satırlar silinir.

**Bilinçli sınırlar:**
- **1v1.** `World` tam olarak iki uçak tutar; roller `roles.hpp`'de sabittir.
- **Karar mekanizması yok.** Manevra ve atış `step` içinde elle seçilir. Eklenecek katman
  `world`'e bakıp `Maneuvers` içinden seçim yapacak; manevralar değişmeyecek.
- **Bogey sürülmez.** Python her uçak için komut istediği için bogey'e YAML başlangıcı aynen
  gider (hold). El ile sürüş sonra eklenecek.
- **Bogey hakkında her şey bilinir.** Konumu, füze sayısı ve profili JSON'dan tam gelir.
  Gerçekte bunların bir kısmı ancak radar görürse bilinir. Otonomi yazarken akılda tut.
- **Zaman aşımı yok.** Taraflardan biri takılırsa diğeri bekler.

**Eski kod:** 17 Ekim 2026'da `controller.*` ve `objects.*` kaldırıldı; yapı baştan, adım
adım kuruldu. Eski hali git geçmişinde: `git show 5f57dc6:src/otopilot/controller.cpp`.
