# Otopilot geliştirme rehberi

Uçak davranışını değiştirmek için fighter tarafında [otopilot_fighter.cpp](control/otopilot_fighter.cpp), hostile tarafında [otopilot_hostile.cpp](control/otopilot_hostile.cpp) düzenlenir. İkisi de kendi `step` fonksiyonunda tek bir switch'le yazılmış birer durum makinesidir. Kod kuralları için: [KOD_STANDARTLARI.md](KOD_STANDARTLARI.md).

Bir step'te sıra şöyledir (`Sim::step`):

1. `StateReader` ölçümleri ve Python'un son uyguladığı komutu (`own.command`) okur.
2. `World::update` düşman atamasını, füze bilgilerini ve her uçağın `picture`'ını hesaplar.
3. Önceki step'in atış emri sıfırlanır; her otopilot `step(own, enemy)` ile aktif manevrayı uçurur, ardından geçiş kurallarını çalıştırır.
4. `CommandWriter` `own.command`'ı ve atış emirlerini Python'a gönderir.

## Ölçüm ve durum bilgisine erişim

```cpp
own.kinematics.heading_deg     // ölçülen burun yönü, ±180
own.kinematics.altitude_ft     // ölçülen irtifa, ft
own.kinematics.tas_kt          // gerçek hava hızı, KTAS
own.picture.range_3d_nm        // atanmış düşmana mesafe, NM (World::update)
own.picture.ata_deg            // + sağda, - solda
own.picture.mar_nm             // NM, düşmanın ana silahına karşı Minimum Abort Range
own.picture.threat             // const Missile*: bana gelen en yakın düşman füzesi; yoksa nullptr
own.picture.threats            // std::vector<const Missile*>: bana gelen bütün düşman füzeleri
own.picture.is_spiked          // bana gelen düşman füzesi var mı
own.picture.me_in_enemy_mar    // ben düşmanın MAR'ının içinde miyim (düşmanın hesabından kopyalanır)

own.weapons.wez                // benim WEZ'im (profile.main_weapon'a göre)
own.weapons.nez_nm             // benim NEZ'im: düşman şimdi out yapsa füzemden kaçamaz
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

`FlightCommand`, uçağın anlık ölçümü değil ulaşmasını istediğin hedeftir. Setter'lar yönü ±180'e sarar, gazı 0..1'e sıkıştırır, g sınırını profildeki `max_g`'ye indirir; geçersiz komut oluşamaz. Gaz sayıyla değil seviyeyle yazılır: `throttle::IDLE`, `throttle::DEFAULT` (0.45), `throttle::MRT` (0.5, askeri güç), `throttle::MAX_THRUST` (1.0, tam art yakıcı). Gazı şimdilik manevralar kendi içinde yazar. Her step `own.command` Python'un son uyguladığı komutla başlar. Bir değeri değiştirirsen başka bir değer yazılana kadar o değer kalır. g sınırı ise Python'dan geri gelmez: her step yok başlar, gerekiyorsa manevra koyar.

## Bir aşamanın geçişini yaz

Durumlar iki otopilotun ortak listesi `OtopilotState`'tedir (`control/otopilot_state.hpp`). `step` içindeki tek switch'te her durum bir bloktur: önce o durumun manevrası `maneuvers_.fly` ile uçulur, sonra çıkış koşullarına bakılır. `go_to(yeni_durum)` yalnız state'i değiştirir. Manevra o state'in bloğunda seçilir; ilk kullanımda ve manevra değiştiğinde `Maneuvers::fly` giriş hazırlığını yapar. Atış manevra değildir: yalnız `own.weapons.fire` emri verilir, önceki manevranın komutu sürer. Örnek durum makinesi:

```cpp
// otopilot_fighter.cpp
void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
{
    switch (state_)
    {
    case OtopilotState::Commit:
        maneuvers_.fly(own, maneuvers_.commit);
        if (condition::enemy_in_my_wez(own) && own.weapons.fire(own.profile.main_weapon))
        {
            go_to(OtopilotState::FirstSupport);   // taraf sonraki step'te, ilk crank kullanımında seçilir
        }
        break;

    case OtopilotState::FirstSupport:
        maneuvers_.fly(own, maneuvers_.crank);
        if (condition::in_mar(own))
        {
            go_to(OtopilotState::FirstEscape);
        }
        break;

    case OtopilotState::FirstEscape:
        maneuvers_.fly(own, maneuvers_.out);   // gazı Out kendisi yazar (tam art yakıcı)
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            go_to(OtopilotState::Commit);      // Commit kendi gazını (DEFAULT) yazar
        }
        break;

    default:
        break;
    }
}
```

`Maneuvers::fly` fazı manevradan alır (`own.phase = manevra.phase()`) ve komutu yazar. Aynı manevra sürerken `enter()` tekrarlanmaz; araya başka manevra girdikten sonra geri dönülürse yeniden çalışır. Aynı manevrayı kullanan iki state arasında geçiş, yeniden giriş sayılmaz. İlk manevranın girişi de otomatik yapılır.

Koşulları `control/conditions.hpp`'ye isimli fonksiyonlar olarak ekle. Karar yapısı ileride karar ağacına geçerse koşullar aynen kullanılır. Hazır koşullar: `in_mar`, `in_dor`, `nose_on_target`, `inside_enemy_mar`, `enemy_in_my_wez`, `enemy_in_my_nez`, `enemy_aspect`, `my_aspect`, `is_spiked`, `own_missile_pitbull(own, fire_order)` (1: ilk attığım füze, 2: ikinci, ...). Hepsi yalnız `own`'u okur.

`enemy_aspect(own)` düşmanın burnunun bana göre durumunu (`picture.ta_deg`), `my_aspect(own)` benim burnumun düşmana göre durumunu (`picture.ata_deg`) `Aspect` olarak verir: `Hot` (≤ 30°), `Flank` (≤ 70°), `Beam` (≤ 110°), `Cold` (ötesi). Sağ/sol ayrılmaz; eşikler `conditions.cpp` içindeki sabitlerdedir.

```cpp
if (condition::enemy_aspect(own) == Aspect::Cold) { ... }   // düşman benden uzaklaşıyor
```

Şu anki fighter ve hostile taktiği şov senaryosudur; akışı ve çalıştırma komutu [OTOPILOT_MIMARISI.md](../../docs/mimari/OTOPILOT_MIMARISI.md)'dedir.

## Manevralar

| Manevra | Faz | Gaz | Bitişi |
|---|---|---|---|
| `hold` | Hold | dokunmaz | sürekli |
| `out` | Out | MAX_THRUST | `status()` → Done: düşman arkada (\|ATA\| > 177°) |
| `commit` | Commit | DEFAULT | sürekli |
| `recommit` | Recommit | MAX_THRUST | `status()` → Done: \|ATA\| atış açısına (30° + 1°) indi |
| `crank` | Crank | DEFAULT | sürekli; `in_radar_sight(own)` düşman radar konisinde mi (\|ATA\| < gimbal). Taraf girişte (`enter`) seçilir, `side()` ile okunur |
| `beam` | Beam | DEFAULT | sürekli |

Notch, Drag, Offset, Flank, Collision, FlyHeading, Snake ve Pursuit (pure/lag/lead) iskelet olarak durur; gövdeleri komutu değiştirmez. Sürekli manevraları otopilot bitirir. Karara yardım eden sorgular (`Crank::in_radar_sight` gibi) manevranın kendisinde durur.

## Atış ve füzeler

```cpp
if (own.weapons.fire(own.profile.main_weapon)) { ... }   // bu step için atış emri oluştu

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

`weapons.fire` sonucu fiziksel isabet ya da Python onayı değildir. İstenen profilde atılmamış slot kalmadıysa ya da henüz eşleşmemiş bir atış bekleniyorsa `false` döner. Füze işaretçilerini sonraki step'e saklama.

## Adımlar arasında bilgi sakla

Otopilota ait sayaç, zaman ya da tercih ilgili otopilot sınıfının `private` üyesi olur. Sabitler birimli isimle `static constexpr` olarak tanımlanır:

```cpp
static constexpr double REFIRE_INTERVAL_S = 5.0;
double last_fire_time_s_ = -1e9;
```

## Yeni manevra ekle

`control/maneuver.hpp`:

```cpp
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

`control/maneuver.cpp`:

```cpp
FlightCommand RightOffset::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı kalır
    command.set_heading_deg(own.picture.bearing_deg + OFFSET_DEG)
           .set_throttle(THROTTLE);
    return command;
}
```

`Maneuvers` sınıfına `RightOffset right_offset;` satırını ekle ve bir durumun bloğunda `maneuvers_.fly(own, maneuvers_.right_offset)` ile uçur. Manevra kendi içinde bitiyorsa `status()`'u, girişte bir şey kilitlemesi gerekiyorsa `enter()`'ı uygula. Yeni bir `Phase` değeri dış sözleşmeyi etkiler: `model/phase.hpp` ile görüntüleyicideki `PHASE_NAMES` listesi birlikte güncellenir.

## Profil veya hesap ekle

| İhtiyaç | Düzenlenecek kaynak |
|---|---|
| Yeni uçak etiketi / radar / yük | `model/aircraft_profiles.hpp`; YAML `type` ile aynı anahtar |
| Füze menzil, ortalama hız, pitbull eşiği, R_cold ve Rmax (WEZ) uyumu | `model/missile_profiles.hpp` |
| Saf konum/hız geometrisi | `calculations/geometry.hpp/.cpp` |
| Dönüş süresi, yarıçapı, yer değişimi | `calculations/turn.hpp/.cpp` |
| WEZ, A/F-pole, TR, out planı, R_cold, NEZ, MAR, MRR, DR, DOR | `calculations/engagement.hpp/.cpp` |
| Füze–hedef hesabı ve füze uçuş uyumu | `calculations/missile_geometry.hpp/.cpp` |
| Hesaplanan alanın uçakta tutulması | `model/picture.hpp` (kaçış) ya da `model/weapons.hpp` (silah) ve `model/world.cpp` |
| Yeni ölçümün Python'dan gelmesi | Python `Simulation.read_state()` ve C++ `StateReader` |

Uçak profilindeki `main_weapon` WEZ, NEZ, F-pole ve karşı tarafın R_cold/MAR hesabında kullanılır. `loadout` slot sırasındadır; Python `ammo` kadar slot baştan alınır. Profilde AIM-7M/AIM-9X yazmak Python füzesinin fizik/güdüm modelini değiştirmez.

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
