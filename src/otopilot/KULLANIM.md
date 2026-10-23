# Otopilot geliştirme rehberi

Uçak davranışını değiştirmek için fighter tarafında [otopilot_fighter.cpp](control/otopilot_fighter.cpp), hostile tarafında [otopilot_hostile.cpp](control/otopilot_hostile.cpp) düzenlenir. `Sim` ortak adımı yönetir; taktik `switch` blokları bu iki otopilotun içindedir.

Her iki sınıfta da `step` imzası aynı yapıdadır:

```cpp
FlightCommand step(
    Aircraft& own,
    const World& world,
    const FlightCommand& current);
```

Gerçek parametre adı fighter tarafında `fighter`, hostile tarafında `hostile`'dir. Örnekler fighter içindir; aynı işlemler hostile'in kendi dosyasına uygulanabilir.

## Ölçüm ve dünya bilgisine erişim

```cpp
fighter.kinematics.heading_deg_180   // ölçülen burun yönü
fighter.kinematics.altitude_ft       // ölçülen irtifa, ft
fighter.kinematics.tas_kt            // gerçek hava hızı, KTAS
fighter.enemy.range_3d_nm            // World tarafından hesaplanan hedef mesafesi, NM
fighter.enemy.ata_deg                // + sağda, - solda
fighter.enemy.enemy_in_my_wez
world.sim_time_s
world.hostile.health
world.hostile.has_active_missile_against(fighter)
```

Geometri `World::update()` içinde zaten doldurulmuştur. `world` ikinci parametrede salt okunurdur; diğer uçağın komutunu veya aşamasını bu otopilottan değiştirme. Kendi uçuş hedefini `command_`, kendi karar adımını `state_` ile yönet.

## Yön, irtifa ve gaz hedefi

```cpp
command_.heading_deg_180 = 90.0;
command_.altitude_ft = 26000.0;
command_.throttle = 0.8;
```

`FlightCommand`, uçağın anlık ölçümü değil ulaşmasını istediğin hedeflerdir. Otopilot her adım `command_ = current` ile başlar; `current`, Python'un son uyguladığı komuttur. Bir aşamada gazı değiştirirsen sonraki aşamada başka değer yazılana kadar o değer kalır.

C++ heading aralığı `(-180, 180]`'dir. Yön hesabında `wrap_180()` kullan. JSON'a çıkarken `CommandWriter` yönü `0..360` biçimine, feet irtifasını metreye çevirir. C++ mesafeleri NM, hızları kt kullanır.

## Hazır manevrayı kullan

```cpp
fighter.phase = Phase::Crank;
command_ = maneuvers_.crank_right.compute(fighter, command_);
```

Diğer seçenekler `hold`, `in`, `out`, `crank_left`, `notch` üyeleridir. Bu manevralar mevcut uygulamada yön alanını değiştirir veya komutu korur. Gaz ve irtifa gerekiyorsa aynı aşamada ayrıca yazılır.

## Bir aşamanın geçişini yaz

İlgili otopilotun `switch (state_)` bloğunda:

```cpp
case 2:
    fighter.phase = Phase::Crank;
    command_ = maneuvers_.crank_right.compute(fighter, command_);

    if (const Missile* missile = fighter.last_fired();
        missile && missile->target.pitbull)
    {
        state_ = 3;
    }
    break;
```

`state_ = 3`, sonraki `step` çağrısının 3. Aşamayı çalıştırmasını sağlar. `Phase` kayda çıkan etikettir; iç adım numarasıyla aynı olmak zorunda değildir. Yeni `Phase` değeri dış sözleşmeyi etkiler: `model/aircraft.hpp` enum'u ile görüntüleyicideki `web/app.js` `PHASE_NAMES` listesi birlikte güncellenir.

## Atış ve tehdit sorgusu

```cpp
if (fighter.fire_missile(FOX3))
{
    // Bu adım için atış isteği oluşturuldu.
}
```

Dönüş değeri fiziksel isabet veya Python onayı değildir. İstenen tipte atılmamış slot kalmadıysa ya da henüz eşleşmemiş atış bekleniyorsa `false` gelir. Yeniden deneme istiyorsan geçişini dönüş değerine başla; mevcut deneme taktikleri bunu yapmıyor.

```cpp
const Missile* last = fighter.last_fired();
if (last && last->has_kinematics)
{
    double range_nm = last->target.range_nm;
    (void)range_nm;
}

bool threatened = world.hostile.has_active_missile_against(fighter);
int remaining = fighter.missiles_left();
```

Son füze işaretçisini sonraki adıma saklama. Hazır veya henüz Python ölçümü gelmemiş bir füzenin kinematiğini uçuş verisi gibi kullanma. Tehdit sorgusu bütün havadaki ve hedef kimliği eşleşen füzeleri inceler.

## Adımlar arasında bilgi sakla

Fighter'a ait sayaç, zaman veya tercih `control/otopilot_fighter.hpp` içindeki sınıfa eklenir. Hostile'inki kendi sınıfına eklenir. Ortak bir ölçüm veya hesap değilse `World`e eklemeye gerek yoktur.

```cpp
// OtopilotFighter private üyeleri:
double last_fire_time_s_ = -1e9;
```

```cpp
// Uygun karar aşamasında:
if (world.sim_time_s - last_fire_time_s_ >= 5.0 &&
    fighter.fire_missile(FOX3))
{
    last_fire_time_s_ = world.sim_time_s;
}
```

Bu bir geliştirme örneğidir; mevcut taktiğe otomatik uygulanmaz.

## Yeni manevra ekle

`control/maneuver.hpp` içine ortak arayüzü uygulayan sınıf ekle:

```cpp
class Offset : public Maneuver
{
public:
    FlightCommand compute(
        const Aircraft& own,
        const FlightCommand& current) override;
};
```

`control/maneuver.cpp` içine hesap gövdesini yaz:

```cpp
FlightCommand Offset::compute(
    const Aircraft& own,
    const FlightCommand& current)
{
    FlightCommand command = current;
    command.heading_deg_180 = wrap_180(own.enemy.bearing_deg + 30.0);
    return command;
}
```

`Maneuvers` yapısına `Offset offset;` ekle ve ilgili otopilot aşamasında çağır:

```cpp
command_ = maneuvers_.offset.compute(fighter, command_);
```

İki otopilot ayrı `Maneuvers` nesneleri tuttuğu için manevranın kendi hafızası da ayrıdır. Yeni komutu sıfırdan kurmak yerine `current` kopyasıyla başlamak irtifa ve gazı korur.

## Profil veya hesap ekle

| İhtiyaç | Düzenlenecek kaynak |
|---|---|
| Yeni uçak etiketi / radar / yük | `model/aircraft_profiles.hpp`; YAML `type` ile aynı anahtar |
| Füze menzil, ortalama hız ve pitbull eşiği | `model/missile_profiles.hpp` |
| Saf konum/hız geometrisi | `calculations/geometry.hpp/.cpp` |
| Profil ve angajman modeli | `calculations/engagement.hpp/.cpp` |
| Füze–hedef hesabı | `calculations/missile_geometry.hpp/.cpp` |
| Hesaplanan alanın uçakta tutulması | `model/aircraft.hpp` ve `model/world.cpp` |
| Yeni ölçümün Python'dan gelmesi | Python `Simulation.read_state()` ve C++ `StateReader` |

Uçak profilindeki `loadout` slot sırasındadır; Python `ammo` kadar slot baştan alınır. Profilde FOX1/FOX2 yazmak Python füzesinin fizik/güdüm modelini değiştirmez.

## Bir hesabı kayda ve arayüze çıkar

1. Alanı C++ modeline ekle ve `World::update()` üzerinden doldur.
2. `io/calculation_writer.cpp` içindeki `to_json()` çıktısına birimi adında olan anahtar ekle.
3. Aynı oturum boyunca aynı hesap alan kümesini gönder; `Recorder` değişen kolonları reddeder.
4. Alanı [veri referansında](../../docs/veri_info/SIMULASYON_VERI_REFERANSI.md) tanımla.
5. Görüntüleyicide göstermek istiyorsan `web/app.js` panelini veya sahne katmanını ayrıca düzenle.

Python hesap zamanını kendisi ekler; C++ hesap satırına `time_s` gönderilmez. Alanın CSV'de olması arayüzde otomatik yeni bir satır veya çizim oluşturmaz.

## Derleme ve gözlem

Proje kökünde `cmake --build src/otopilot/build` çalıştır. Yeni `.cpp` dosyasını uygun kaynak klasörüne kaydet; CMake dosyayı algılar. Uçuş sonrasında yeni oturum klasörünü görüntüleyicide aç.

Konsol gözlemi için `control/sim.cpp` içindeki `// log_.print(world_);` satırını açabilirsin. Log yaklaşık her simülasyon saniyesinde bir satır yazar. Ayrıntılı zaman ve alan incelemesi için CSV dosyaları esas alınır.
