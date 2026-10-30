# Otopilot geliştirme rehberi

Uçak davranışını değiştirmek için fighter tarafında [otopilot_fighter.cpp](control/otopilot_fighter.cpp), hostile tarafında [otopilot_hostile.cpp](control/otopilot_hostile.cpp) düzenlenir. İkisi de kendi `step` fonksiyonunda tek bir switch'le yazılmış birer durum makinesidir. Kod kuralları için: [KOD_STANDARTLARI.md](KOD_STANDARTLARI.md).

Bir step'te sıra şöyledir (`Sim::step`):

1. `StateReader` ölçümleri ve Python'un son uyguladığı komutu (`own.command`) okur.
2. `World::update` düşman atamasını, füze bilgilerini ve her uçağın `picture`'ını hesaplar.
3. Her otopilot `step(own, enemy)` ile aktif manevrayı uçurur, ardından geçiş kurallarını çalıştırır.
4. `CommandWriter` `own.command`'ı ve atış emirlerini Python'a gönderir.

## Ölçüm ve durum bilgisine erişim

```cpp
own.kinematics.heading_deg     // ölçülen burun yönü, ±180
own.kinematics.altitude_ft     // ölçülen irtifa, ft
own.kinematics.tas_kt          // gerçek hava hızı, KTAS
own.picture.range_3d_nm        // atanmış düşmana mesafe, NM (World::update)
own.picture.ata_deg            // + sağda, - solda
own.picture.mar_nm             // NM, düşmanın ana silahına karşı Minimum Abort Range
own.picture.threat             // std::optional<ThreatInfo>: bana gelen en acil füze
enemy.picture.mar_nm           // düşmanın kendi hesabı (ör. hostile senaryosu bunu kullanır)

own.weapons.wez                // benim WEZ'im (profile.main_weapon'a göre)
own.weapons.nez_nm             // benim NEZ'im: düşman şimdi out yapsa füzemden kaçamaz
own.weapons.enemy_in_wez       // düşman WEZ'imde mi
own.weapons.enemy_in_nez       // düşman NEZ'imde mi
own.weapons.f_pole_nm          // std::optional<double>: füze yetişemiyorsa değer yok
```

İki taraf ayrıdır: `picture` atanmış düşmana göre geometri ve **kaçış** (düşmanın füzesine karşı), `weapons` ise **benim füzem** (füze slotları ve ana silahımın düşmana karşı durumu). Silah hesapları profildeki `main_weapon` ile yapılır; füze slotlarına ya da kalan füze sayısına bakılmaz.

`picture` ve `weapons`'ın hesaplanan alanları yalnız okunur. Yeni bir değer gerekiyorsa `calculations/`'a fonksiyon, `model/picture.hpp` ya da `model/weapons.hpp`'ye alan eklenir ve `World::update` içinde doldurulur.

`optional` kullanımı (yalnız gerçekten olmayabilen değerlerde: tehdit, kalan süre, F-pole):

```cpp
if (const auto f_pole_nm = own.weapons.f_pole_nm)   // değer varsa
{
    if (*f_pole_nm > 15.0) { ... }                   // * ile değeri al
}
const double ttg_s = own.picture.threat ? own.picture.threat->time_to_go_s.value_or(1e9) : 1e9;
```

## Yön, irtifa ve gaz hedefi

```cpp
own.command.set_heading_deg(own.picture.bearing_deg + 30.0)
           .set_altitude_ft(26000.0)
           .set_throttle(0.8)
           .set_g_limit(4.0);
```

`FlightCommand`, uçağın anlık ölçümü değil ulaşmasını istediğin hedeftir. Setter'lar yönü ±180'e sarar, gazı 0..1'e ve g sınırını 1.5..9'a sıkıştırır; geçersiz komut oluşamaz. Her step `own.command` Python'un son uyguladığı komutla başlar. Bir değeri değiştirirsen başka bir değer yazılana kadar o değer kalır. g sınırı ise Python'dan geri gelmez: her step yok başlar, gerekiyorsa manevra koyar.

## Bir aşamanın geçişini yaz

Durumlar `enum class` ile yazılır. `step` içindeki tek switch'te her durum bir bloktur: önce o durumun manevrası `fly` ile uçulur, sonra çıkış koşullarına bakılır. Geçiş `go_to(yeni_durum, yeni_manevra, own)` ile yapılır; yeni manevranın `enter()`'ı o anda çağrılır, kendisi bir sonraki step'te uçulur:

```cpp
// otopilot_fighter.hpp
enum class FighterState { Hold, Crank, Out };

// otopilot_fighter.cpp
void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
{
    switch (state_)
    {
    case FighterState::Hold:
        fly(own, maneuvers_.hold);
        if (condition::enemy_in_my_wez(own) && own.weapons.fire(AIM120C))
        {
            go_to(FighterState::Crank, maneuvers_.crank, own);   // taraf Crank::enter'da seçilir
        }
        break;

    case FighterState::Crank:
        fly(own, maneuvers_.crank);
        if (condition::in_mar(own))
        {
            go_to(FighterState::Out, maneuvers_.out, own);
        }
        break;

    case FighterState::Out:
        fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            own.command.set_throttle(0.45);   // geçiş anında komut düzeltmesi
            go_to(FighterState::Hold, maneuvers_.hold, own);
        }
        break;
    }
}
```

`Otopilot` iskeleti fazı manevradan alır (`own.phase = manevra.phase()`), yeni duruma geçince manevranın `enter()`'ını çağırır. Yeni manevra bir sonraki step'te uçulur. Bu kodlarda `enter()` çağırmayı ya da fazı elle yazmayı unutma riski yoktur.

Koşulları `control/conditions.hpp`'ye isimli fonksiyonlar olarak ekle. Karar yapısı ileride karar ağacına geçerse koşullar aynen kullanılır. Hazır koşullar: `in_mar`, `inside_enemy_mar`, `enemy_in_my_wez`, `enemy_in_my_nez`, `enemy_cold`, `range_within`, `last_shot_before_mar`, `threat_inbound`, `threat_within_s`, `own_missile_pitbull`.

Şu anki fighter ve hostile taktiği şov senaryosudur; akışı ve çalıştırma komutu [OTOPILOT_MIMARISI.md](../../docs/mimari/OTOPILOT_MIMARISI.md)'dedir.

## Manevralar

| Manevra | Faz | Bitişi |
|---|---|---|
| `hold` | Hold | sürekli |
| `fire` | Fire | sürekli (komut değişmez; tek step'lik atış durumu için) |
| `out` | Out | `status()` → Done: düşman arkada (\|ATA\| > 177°) |
| `in` | In | sürekli |
| `crank` | Crank | sürekli; `in_position(own)` düşman gimbal sınırına oturdu mu. Taraf girişte (`enter`) `crank_side_for(own)` ile seçilir, `side()` ile okunur |
| `beam` | Beam | sürekli |

Sürekli manevraları otopilot bitirir. Karara yardım eden sorgular (`Crank::in_position` gibi) manevranın kendisinde durur.

## Atış ve füzeler

```cpp
if (own.weapons.fire(AIM120C)) { ... }   // bu step için atış emri oluştu

const Missile* supporting = own.weapons.active_missile();   // havadaki son füzem; yoksa nullptr
if (supporting != nullptr && supporting->target.is_pitbull) { ... }

if (own.picture.threat)                               // bana gelen en acil düşman füzesi
{
    const ThreatInfo& threat = *own.picture.threat;
    // threat.range_nm, threat.time_to_go_s (optional), threat.bearing_deg, threat.aspect_deg, threat.is_pitbull
}
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
class Offset : public Maneuver
{
public:
    Phase phase() const override { return Phase::In; }
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double OFFSET_DEG = 30.0;   // derece, bearing'e eklenen açı
};
```

`control/maneuver.cpp`:

```cpp
FlightCommand Offset::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa ve gaz aynı kalır
    command.set_heading_deg(own.picture.bearing_deg + OFFSET_DEG);
    return command;
}
```

`Maneuvers` yapısına `Offset offset;` satırını ekle ve bir durumun bloğunda `fly(own, maneuvers_.offset)` ile uçur. Manevra kendi içinde bitiyorsa `status()`'u, girişte bir şey kilitlemesi gerekiyorsa `enter()`'ı uygula. Yeni bir `Phase` değeri dış sözleşmeyi etkiler: `model/phase.hpp` ile görüntüleyicideki `PHASE_NAMES` listesi birlikte güncellenir.

## Profil veya hesap ekle

| İhtiyaç | Düzenlenecek kaynak |
|---|---|
| Yeni uçak etiketi / radar / yük | `model/aircraft_profiles.hpp`; YAML `type` ile aynı anahtar |
| Füze menzil, ortalama hız, pitbull eşiği, R_cold uyumu | `model/missile_profiles.hpp` |
| Saf konum/hız geometrisi | `calculations/geometry.hpp/.cpp` |
| Dönüş süresi, yarıçapı, yer değişimi | `calculations/turn.hpp/.cpp` |
| WEZ, out planı, R_cold, NEZ, MAR, DOR | `calculations/engagement.hpp/.cpp` |
| Füze–hedef hesabı | `calculations/missile_geometry.hpp/.cpp` |
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

Konsol gözlemi için `control/sim.cpp` içindeki `// log_.print(world_);` satırını açabilirsin. Ayrıntılı zaman ve alan incelemesi için CSV dosyaları esas alınır.
