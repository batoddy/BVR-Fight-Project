# Otopilot geliştirme rehberi

İki uçak da aynı durum makinesini, [otopilot.cpp](control/legacy/otopilot.cpp)'deki `Otopilot` sınıfını kendi görev konseptiyle (`Mission`, [sim.hpp](control/sim.hpp)) kullanır. Her durumun ne yapacağını [decisions.cpp](control/legacy/decisions.cpp)'deki karar fonksiyonları seçer; kurallar [KARAR_TASARIMI.md](control/KARAR_TASARIMI.md)'de. Kod kuralları için: [KOD_STANDARTLARI.md](KOD_STANDARTLARI.md).

Bir step'te sıra şöyledir (`Sim::step`):

1. `StateReader` ölçümleri ve Python'un son uyguladığı komutu (`own.command`) okur.
2. `World::update` düşman atamasını, füze bilgilerini ve her uçağın `picture`'ını hesaplar.
3. Önceki step'in atış emri sıfırlanır; her otopilot `step(own)` ile önce kesintilere (ABORT, tehdit) bakar, sonra o durumun manevrasını uçurup kararına göre geçiş yapar.
4. `CommandWriter` `own.command`'ı ve atış emirlerini Python'a gönderir.

## Ölçüm ve durum bilgisine erişim

```cpp
own.kinematics.heading_deg     // ölçülen burun yönü, ±180
own.kinematics.altitude_ft     // ölçülen irtifa, ft
own.kinematics.tas_kt          // gerçek hava hızı, KTAS
own.picture.range_3d_nm        // atanmış düşmana mesafe, NM (World::update)
own.picture.ata_deg            // + sağda, - solda
own.picture.mar_nm             // NM, düşmanın ana silahına karşı Minimum Abort Range
own.picture.threat             // const Missile*: bana gelen en yakın güdümlü düşman füzesi; yoksa nullptr
own.picture.threats            // std::vector<const Missile*>: bana gelen bütün düşman füzeleri
own.picture.is_spiked          // bana gelen düşman füzesi var mı

own.weapons.wez                // benim WEZ'im (profile.main_weapon'a göre)
own.weapons.enemy_in_wez       // düşman WEZ'imde mi
own.weapons.enemy_in_nez       // düşman NEZ'imde mi
own.weapons.f_pole_nm          // std::optional<double>: füze yetişemiyorsa değer yok
```

İki taraf ayrıdır: `picture` atanmış düşmana göre geometri ve **kaçış** (düşmanın füzesine karşı), `weapons` ise **benim füzem** (füze slotları ve ana silahımın düşmana karşı durumu). Silah hesapları profildeki `main_weapon` ile yapılır; füze slotlarına ya da kalan füze sayısına bakılmaz.

`picture` ve `weapons`'ın hesaplanan alanları yalnız okunur. Yeni bir değer gerekiyorsa `calculations/`'a fonksiyon, `model/picture.hpp` ya da `model/weapons.hpp`'ye alan eklenir ve `World::update` içinde doldurulur.

`optional` kullanımı (yalnız gerçekten olmayabilen değerlerde: F-pole, catch time):

```cpp
if (const auto f_pole_nm = own.weapons.f_pole_nm)   // değer varsa
{
    if (*f_pole_nm > 15.0) { ... }                   // * ile değeri al
}
if (own.picture.threat != nullptr) { ... }           // bana gelen füze var mı (tehdit adres, optional değil)
```

## Yön, irtifa ve gaz hedefi

```cpp
own.command.set_heading_deg(own.picture.bearing_deg + 30.0)
           .set_altitude_ft(26000.0)
           .set_throttle(throttle::MRT)
           .set_g_limit(4.0, own.profile.max_g);
```

`FlightCommand`, uçağın anlık ölçümü değil ulaşmasını istediğin hedeftir. Setter'lar yönü ±180'e sarar, gazı 0..1'e sıkıştırır, g sınırını profildeki `max_g`'ye indirir; geçersiz komut oluşamaz. Gaz sayıyla değil seviyeyle yazılır: `throttle::IDLE`, `throttle::CRANK` (0.35), `throttle::DEFAULT` (0.45), `throttle::MRT` (0.5, askeri güç), `throttle::MAX_THRUST` (1.0, tam art yakıcı). Gazı şimdilik manevralar kendi içinde yazar. Her step `own.command` Python'un son uyguladığı komutla başlar. Bir değeri değiştirirsen başka bir değer yazılana kadar o değer kalır. g sınırı ise Python'dan geri gelmez: her step yok başlar, gerekiyorsa manevra koyar.

## Bir aşamanın geçişini yaz

Durumlar `OtopilotState`'tedir (`control/legacy/otopilot_state.hpp`). Her durumun kendi `step_*` fonksiyonu vardır: önce manevrası `maneuvers_.fly` ile uçulur, sonra o durumun karar fonksiyonu (`decide::*`) çağrılır ve seçime göre geçilir. Kayıtlı geçiş `go_to(own, yeni_durum, metin, neden)` ile yapılır: karar metnini `own.decision`'a yazar ve durumu değiştirir. Atış manevra değildir: Shot durumu yalnız `own.weapons.fire()` emrini verir, önceki manevranın komutu sürer.

```cpp
// otopilot.cpp
void Otopilot::step_support(Aircraft& own)
{
    maneuvers_.fly(own, support_maneuver(own));                      // düşman cold ise kovalama, değilse crank
    const Decision<SupportChoice> decision = decide::support(own, mission_);
    switch (decision.choice)
    {
    case SupportChoice::Continue:
        break;
    case SupportChoice::Out:
        go_to(own, OtopilotState::Out, "SUPPORT → OUT", decision.reason);
        break;
    ...
    }
}
```

Yeni bir karar: seçimi `decisions.hpp`'deki enum'a, kuralı `decisions.cpp`'deki fonksiyona, geçişi ilgili `step_*`'a eklenir.

`Maneuvers::fly` fazı manevradan alır (`own.phase = manevra.phase()`) ve komutu yazar. Aynı manevra sürerken `enter()` tekrarlanmaz; araya başka manevra girdikten sonra geri dönülürse yeniden çalışır. Aynı manevrayı kullanan iki state arasında geçiş, yeniden giriş sayılmaz. İlk manevranın girişi de otomatik yapılır.

Koşulları `control/conditions.hpp`'ye isimli fonksiyonlar olarak ekle. Karar yapısı ileride karar ağacına geçerse koşullar aynen kullanılır. Hazır koşullar: `in_mar`, `in_dor`, `enemy_in_my_wez`, `enemy_in_my_tr`, `enemy_aspect`, `my_aspect`, `has_live_missile`, `threat_alive`, `supporting`, `is_spiked` ve diğerleri (tam liste `conditions.hpp`'de). Hepsi yalnız `own`'u okur.

`enemy_aspect(own)` düşmanın burnunun bana göre durumunu (`picture.ta_deg`), `my_aspect(own)` benim burnumun düşmana göre durumunu (`picture.ata_deg`) `Aspect` olarak verir: `Hot` (≤ 30°), `Flank` (≤ 70°), `Beam` (≤ 110°), `Cold` (ötesi). Sağ/sol ayrılmaz; eşikler `conditions.cpp` içindeki sabitlerdedir.

```cpp
if (condition::enemy_aspect(own) == Aspect::Cold) { ... }   // düşman benden uzaklaşıyor
```

Fighter ve hostile'ın görev konseptleri `sim.hpp`'deki `FIGHTER_MISSION` ve `HOSTILE_MISSION` sabitleridir; akış ve çalıştırma komutu [OTOPILOT_MIMARISI.md](../../docs/mimari/OTOPILOT_MIMARISI.md)'dedir.

## Manevralar

| Manevra | Faz | Gaz | g sınırı | Durumu |
|---|---|---|---|---|
| `hold` | Hold | DEFAULT | 4 | girişte yön/irtifayı kilitler; ±2° ve ±100 ft içinde `Satisfied`, sapınca düzeltmeye devam eder |
| `out` | Out | dönüşte MAX_THRUST, Done olunca MRT (yeniden girişe kadar) | `out_g` | `status()` → Done: düşman arkada (\|ATA\| > 177°) |
| `commit` | Commit | DEFAULT | 4 | burun hedefe ±2° içinde bakıyorsa `Satisfied` |
| `recommit` | Recommit | MAX_THRUST | `out_g` | `status()` → Done: \|ATA\| atış açısına (30° + 1°) indi; ilk varıştan sonra hedef 30°'de kalır |
| `crank` | Crank | CRANK | 3 | sürekli; seçilen tarafta hedef ATA'ya ±2° yakın ve radar içindeyse `Satisfied`. Taraf girişte seçilir; `in_radar_sight(own)` ve `side()` ayrıca sorgulanabilir |
| `beam` | Beam | DEFAULT | 4 | seçilen tarafta ATA ±90° hedefine ±2° içinde oturunca `Satisfied`; taraf girişte seçilir |
| `flank` | Flank | DEFAULT | 4 | sürekli; `set(target_ta_deg, tolerance_deg = 3)` ile verilen işaretli TA tolerans içindeyse `Satisfied`, dışında `Running` |
| `drag` | Drag | MRT | `out_g` | tehdit varken `Running`, kaçılacak tehdit kalmazsa `Done` |
| `notch` | Notch | DEFAULT | 4 | tehdit varken `Running`, kaçılacak tehdit kalmazsa `Done` |
| `offset` | Offset | DEFAULT | 4 | `set(offset_deg, side)`; seçilen tarafta hedef ATA'ya ±2° içinde `Satisfied` |
| `fly_heading` | FlyHeading | DEFAULT | 4 | `set(heading_deg)`; hedef yöne ±2° içinde `Satisfied` |
| `pursuit` | ilgili Pursuit fazı ya da Collision | `set(mode, throttle)`; varsayılan DEFAULT | 4 | `set(PursuitMode::Pure/Lag/Lead)`: modun güncel hedef yönüne ±2° içinde `Satisfied`. `set(PursuitMode::Collision)`: yatay menzil sıfır değil, kapanma pozitif ve LOS hızı ±0,1°/s içindeyse `Satisfied`. Kovalama (CHASE) Pure + MRT kullanır |

g sınırları profildeki `max_g`'yi aşamaz (`set_g_limit(g, own.profile.max_g)`).

`Running`: koşul henüz sağlanmıyor; `Satisfied`: koşul şu anda sağlanıyor ve manevra devam ediyor; `Done`: bitiş koşulu oluştu. `Satisfied` kalıcı bir varış bayrağı değildir; açı bozulursa tekrar `Running` olur. `is_satisfied(own)` yalnız `Satisfied` için true döner. `status()` ve `is_satisfied()` komut üretmez, otomatik geçiş yapmaz.

```cpp
maneuvers_.flank.set(40.0);      // düşmanın burnunun 40° sağı; -40.0 soludur, varsayılan tolerans ±3°
maneuvers_.fly(own, maneuvers_.flank);
if (maneuvers_.flank.is_satisfied(own)) { /* yan açı şu an sağlanıyor */ }
if (maneuvers_.drag.status(own) == ManeuverStatus::Done) { /* tehdit kalmadı */ }

maneuvers_.offset.set(40.0, ManeuverSide::Right); // bearing'in 40° sağı; hedef ATA -40°
maneuvers_.fly(own, maneuvers_.offset);
```

Flank, mevcut yatay menzilde hedefin istenen yanına bir takip noktası koyar; hedefin yer hızını ve konum hatasını kullanarak yön üretir. Tepki süresi 20 s'dir. Hız eşleme ve hedef dönüş hızını öngörme yoktur; farklı hızlarda veya sert hedef dönüşlerinde açı hatası oluşabilir ve `is_satisfied()` false döner.

Pursuit, otopilotla yeniden denemek için eski LOS işaretiyle taraf seçimine alınmıştır. Lag LOS hareketinin gerisine 15°, Lead önüne 20° sabit sapma kullanır; LOS hızı tam sıfırsa Pure yönü kullanılır. Bunlar hız oranından hesaplanan kesin önleme açıları değildir. Collision modu kapanırken mevcut yöne LOS hızıyla orantılı düzeltme ekler; LOS sıfırlanınca edinilmiş kesişme yönünü korur. Kapanma yoksa önce hedefe yönelir. Kontrol ve durum sorgusu yatay geometriye aittir; dikey kesişme koşulu aranmaz. 

Manevralar hazırdır. Mevcut gösterim otopilotunun seçim/geçiş kuralları ayrıca belirlenir; yeni manevralar kendiliğinden akışa girmez.

### Doğrulama

```bash
cmake --build src/otopilot/build
src/otopilot/build/units_test.exe
src/otopilot/build/maneuver_test.exe
```

`build/` silindiyse yeniden kurulur. MinGW kullanıcı klasörünün boşluklu yolunda olduğu için kısa yol ve `-B` bayrakları gerekir:

```bash
T="C:/Users/PC_100~3/tools/mingw64"
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="$T/bin/g++.exe" \
      -DCMAKE_MAKE_PROGRAM="$T/bin/mingw32-make.exe" -DCMAKE_AR="$T/bin/ar.exe" -DCMAKE_LINKER="$T/bin/ld.exe" \
      "-DCMAKE_CXX_FLAGS=-B$T/lib/ -B$T/x86_64-w64-mingw32/lib/"
```

## Atış ve füzeler

```cpp
if (own.weapons.fire()) { ... }   // bu step için atış emri oluştu

const Missile* supporting = own.weapons.active_missile();   // havadaki son füzem; yoksa nullptr
for (const Missile* missile : own.weapons.active_missiles()) { ... }   // havadaki bütün füzelerim, atış sırasıyla
if (supporting != nullptr && supporting->target.is_pitbull) { ... }

if (const Missile* threat = own.picture.threat)       // bana gelen en yakın düşman füzesi (düşmanın füzesinin kendisi)
{
    // threat->target.range_nm, threat->target.is_pitbull, threat->target.catch_time_left_s, threat->kinematics.mach
    // Bana göre açılar füzede tutulmaz: geometry::bearing_deg(own.kinematics, threat->kinematics)
}
for (const Missile* threat : own.picture.threats) { ... }   // bütün tehditler
int remaining = own.weapons.missiles_left();
```

`weapons.fire` sonucu fiziksel isabet ya da Python onayı değildir. Hazır füze kalmadıysa ya da bu step zaten emir verildiyse `false` döner. Hangi slotun atılacağını sim seçer (ilk hazır füze); C++ atılan slotu sonraki state'te görür. Füze işaretçilerini sonraki step'e saklama.

## Adımlar arasında bilgi sakla

Otopilota ait sayaç, zaman ya da tercih `Otopilot` sınıfının `private` üyesi olur. Sabitler birimli isimle `static constexpr` olarak tanımlanır:

```cpp
static constexpr double REFIRE_INTERVAL_S = 5.0;
double last_fire_time_s_ = -1e9;
```

## Yeni manevra ekle

`control/maneuvers/right_offset.hpp`:

```cpp
#pragma once

#include "control/maneuvers/maneuver.hpp"

// Düşmanın 30° sağına uç.
class RightOffset : public Maneuver
{
public:
    Phase phase() const override { return Phase::Offset; }
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double OFFSET_DEG = 30.0;   // derece, bearing'e eklenen açı
    static constexpr double THROTTLE = throttle::DEFAULT;
};
```

`control/maneuvers/right_offset.cpp`:

```cpp
#include "control/maneuvers/right_offset.hpp"

FlightCommand RightOffset::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı kalır
    command.set_heading_deg(own.picture.bearing_deg + OFFSET_DEG)
           .set_throttle(THROTTLE);
    return command;
}
```

`control/maneuvers/maneuvers.hpp`'ye `#include "control/maneuvers/right_offset.hpp"` ve `Maneuvers` sınıfına `RightOffset right_offset;` satırını ekle ve bir durumun bloğunda `maneuvers_.fly(own, maneuvers_.right_offset)` ile uçur. Manevra kendi içinde bitiyorsa `status()`'u, girişte bir şey kilitlemesi gerekiyorsa `enter()`'ı uygula. Yeni bir `Phase` değeri dış sözleşmeyi etkiler: `model/phase.hpp` ile görüntüleyicideki `PHASE_NAMES` listesi birlikte güncellenir.

## Profil veya hesap ekle

| İhtiyaç | Düzenlenecek kaynak |
|---|---|
| Yeni uçak etiketi / radar / yük | `model/profiles/aircraft_profiles.hpp`; YAML `type` ile aynı anahtar |
| Füze menzil, ortalama hız, pitbull eşiği, R_cold ve Rmax (WEZ) uyumu | `model/profiles/missile_profiles.hpp` |
| Saf konum/hız geometrisi | `calculations/geometry.hpp/.cpp` |
| Dönüş süresi, yarıçapı, yer değişimi | `calculations/turn.hpp/.cpp` |
| WEZ, A/F-pole, TR, out planı, R_cold, NEZ, MAR, MRR, DR, DOR | `calculations/engagement.hpp/.cpp` |
| Füze–hedef hesabı ve füze uçuş uyumu | `calculations/missile_geometry.hpp/.cpp` |
| Hesaplanan alanın uçakta tutulması | `model/picture.hpp` (kaçış) ya da `model/weapons.hpp` (silah) ve `model/world.cpp` |
| Yeni ölçümün Python'dan gelmesi | Python `Simulation.read_state()` ve C++ `StateReader` |

Uçak profilindeki `main_weapon` WEZ, NEZ, F-pole ve karşı tarafın R_cold/MAR hesabında kullanılır. Füze slotları `hello` mesajından kurulur: her slotun `designation`'ı `missile_profiles.hpp` tablosundan profil seçer, tabloda yoksa hata. Profilde AIM-7M/AIM-9X yazmak Python füzesinin fizik/güdüm modelini değiştirmez.

## Bir hesabı kayda ve arayüze çıkar

1. Alanı `Picture`'a ya da `Weapons`'a ekle ve `World::update()` içinde doldur.
2. `io/calculation_writer.cpp` içindeki `to_json()` çıktısına birimi adında olan bir anahtar ekle. `optional` değer için tel biçimine karar ver (şu an `value_or(0)`).
3. Aynı oturum boyunca aynı hesap alan kümesini gönder; `Recorder` değişen kolonları reddeder.
4. Alanı [veri referansında](../../docs/veri_info/SIMULASYON_VERI_REFERANSI.md) tanımla.
5. Görüntüleyicide göstermek istiyorsan `BVRGym-Goruntuleyici/web/app.js` panelini ya da sahne katmanını ayrıca düzenle.

## Derleme, test ve gözlem

Proje kökünde `cmake --build src/otopilot/build` çalıştır, ardından `src/otopilot/build/units_test.exe` ile testleri koştur. Yeni `.cpp` dosyasını uygun kaynak klasörüne kaydetmen yeterli; CMake onu `otopilot_core` kütüphanesine ekler.

MinGW kurulum dizininin yolunda boşluk varsa ilk yapılandırmayı kısa (8.3) yol ve `-B` bayraklarıyla yap (PowerShell):

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles" `
  "-DCMAKE_CXX_COMPILER=C:/Users/PC_100~3/tools/mingw64/bin/g++.exe" `
  "-DCMAKE_MAKE_PROGRAM=C:/Users/PC_100~3/tools/mingw64/bin/mingw32-make.exe" `
  "-DCMAKE_CXX_FLAGS=-BC:/Users/PC_100~3/tools/mingw64/lib/ -BC:/Users/PC_100~3/tools/mingw64/x86_64-w64-mingw32/lib/"

cmake --build src/otopilot/build
```

Konsol gözlemi için `control/sim.hpp` içindeki `PRINT_LOG`'u `true` yap. Ayrıntılı zaman ve alan incelemesi için CSV dosyaları esas alınır.
