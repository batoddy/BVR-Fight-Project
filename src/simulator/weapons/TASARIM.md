# Füze tasarımı (Faz 2 + 3)

**Durum (2026-11-12):** Altyapı yazıldı ve çalışıyor: `missile.py` (3-DOF fizik, yaşam döngüsü), `physics/earth.py`, `physics/atmosphere.py`, `data/missiles/*.yaml`, simülasyon ve kayıt entegrasyonu, `tests/test_missile.py`. Ölçülen: 10 km M0.9 atışta yanma sonu (7.75 s) M4.03.

**Kullanıcının dolduracağı üç fonksiyon** (şu an boş; füze güdümsüz düz uçar, isabet olmaz):
- `seeker.py` → `Seeker.update()`
- `guidance.py` → `acceleration_command()`
- `fuze.py` → `detonates()`

Her birinin girdi/çıktısı ve OPRF karşılığı dosyanın içinde yazılı.

**Bekleyen:** Menzil kıyası ve sürükleme kalibrasyonu (analiz s.31–37). Güdüm yazılınca yapılır; güdümsüz füze menzil ölçümüne uygun değil.

Plan: `docs/mimari/SIMULASYON_YENILEME_PLANI.md` §4. Veri: `docs/veri_info/FUZE_VERI_TABLOSU.md`.
Kural: her sayı kaynaklı; kaynak yoksa "geçici + gerekçe" ve raporlanır.

## Veri seçimi (karar 2026-11-12)

- **Motor ve kütle:** `AIM120C5-Performance-Assessment-rev2.pdf` (açık analiz). Toplam 157 kg, itici 51.26 kg, itki 16771.9 N, yanma 7.75 s (s.5–8). Resmi atış kütlesiyle (C-5/6/7 356 lb = 161.5 kg) uyumlu.
- **OPRF değerleri** (2700 lbf × 10 s + 280 lbf × 18 s, 291 lb) `data/missiles/` içinde alternatif profil olarak tutulur.
- **Sürükleme:** PDF s.40 açık bir Cd(M) formülü ve katsayıları veriyorsa o kullanılır. Vermiyorsa OPRF `drag()` formülü (`missile-code.nas`) + OPRF `drag-coeff` / `cross-section-sqft`. Hangisi seçildiyse gerekçesi yazılır.
- **Kurallar:** OPRF (`missile-code.nas`, `station-manager.nas`, `f16-payload.xml` `<aim-120>`).

## Hareket (3-DOF nokta kütle, ECEF)

- Durum: ECEF konum ve hız, kütle. Yerçekimi Dünya merkezine doğru (WGS84). Konumdan jeodezik enlem/boylam/irtifa standart dönüşümle; düz dünya yok (60 km'de ~280 m hata verirdi).
- Hız değişimi: itki (hız yönünde, yanma süresince) − sürükleme − yerçekimi. Kütle yanma boyunca doğrusal azalır.
- Dönüş: güdüm ivme komutu (hıza dik), azami G ile kırpılır. Dönüşün enerji maliyeti: OPRF'teki yöntem (`simplified-induced-drag` / `energyBleed`) çevrilir.
- Atmosfer: ISA (US Standard Atmosphere 1976): yoğunluk, ses hızı.
- Adım: uçakla aynı fizik adımı (1/120 s).

## Güdüm ve arayıcı (OPRF AIM-120 davranışı)

| Durum | Kural | Kaynak |
|---|---|---|
| Veri bağı | Atan uçak hedefi radar görüş bölgesinde tutuyorsa (gövde ekseninden ±60° azimut ve yükseklik) füze hedefin gerçek konumunu ve hızını alır | OPRF `apg-68.nas` field of regard ±60°; `missile-code.nas` `guidance == "sample"` |
| Tahmin | Ölçüm yoksa son ölçüm + hız × geçen süre; irtifa son değerde | `missile-code.nas` `guide()` dead-reckon |
| Pitbull | Füzenin tahmin ettiği hedef konumuna mesafe < 10 NM → kendi arayıcısı | `station-manager.nas` AIM-120 |
| Arayıcı | Hedef füze ekseninden ±67.5° (toplam 135°) ve 38.8 NM içinde → gerçek hedef konumu; değilse güdüm yok, yeniden yakalamayı dener | `f16-payload.xml` seeker-field-deg, max-fire-range-nm (detect-range tanımsız) |
| Notch | Arayıcıda yok (karar 2026-11-12) | — |
| Güdüm yasası | PN, sabit N (OPRF varsayılanı; kodda doğrula) + yerçekimi telafisi | `missile-code.nas` `proportionality-constant` |
| Loft | Yok (OPRF AIM-120 loft-altitude = 0) | `f16-payload.xml` |
| Azami G | 30 | `f16-payload.xml` max-g |
| Güdüm için en düşük hız | Mach 0.6 altında güdüm yok | `f16-payload.xml` min-speed-for-guiding-mach |
| Kurulma | 1.6 s | `f16-payload.xml` arming-time-sec |
| Tapa | Kurulduktan sonra hedefe mesafe ≤ 65 m ve mesafe artmaya başladı → isabet | `missile-code.nas` satır 2027, `max-report-distance` (metre) |
| Bitiş (kayıp) | Yanma bittikten sonra hız < güdüm alt sınırı; ya da kendini imha süresi (110 s); ya da yere çarpma | `f16-payload.xml` self-destruct-time-sec (OPRF notu: "guess") |

İsabette hedefin `health` değeri 1 azalır (eski sim davranışı). Ölen uçak uçmaya devam eder; kullanıcı kararı "şimdilik kalsın".

## Arayüz

- `Simulation.fire(shooter_id, target_id)`: hazır füzeyi atanın konumu, hızı ve yönüyle başlatır → True.
- Her fizik adımında havadaki füzeler ilerler.
- `read_state()["missiles"]`: eski şema. `kinematics`, uçaktaki alanların aynısı; `roll_deg` = 0.
- Kayıt: füze CSV'si eski adla, `AIM-120 AMRAAM ({name}--{key}--{slot}) [{team}].csv`, uçakla aynı sütunlar + `missile_time_s`. Yeni sütun: `guidance_mode` (datalink / inertial / seeker / none).
- Olaylar: `missile_launched`, `missile_hit`, `missile_lost`; yeni: `missile_pitbull`.
