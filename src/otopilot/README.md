# C++ otopilot kaynakları

C++ programı Python simülasyonundan ölçüm alır, ortak hesapları günceller ve iki uçağın komutunu üretir. Giriş `main.cpp`, adımın yöneticisi `Sim`, uçak davranışlarının sahipleri `OtopilotFighter` ile `OtopilotHostile` sınıflarıdır.

```text
otopilot/
├── main.cpp
├── CMakeLists.txt
├── control/
│   ├── sim.hpp / sim.cpp
│   ├── otopilot_fighter.hpp / otopilot_fighter.cpp
│   ├── otopilot_hostile.hpp / otopilot_hostile.cpp
│   └── maneuver.hpp / maneuver.cpp
├── model/
│   ├── aircraft.hpp / aircraft.cpp
│   ├── world.hpp / world.cpp
│   ├── aircraft_profiles.hpp
│   ├── missile_profiles.hpp
│   └── orders.hpp
├── calculations/
│   ├── geometry.hpp / geometry.cpp
│   ├── engagement.hpp / engagement.cpp
│   └── missile_geometry.hpp / missile_geometry.cpp
├── io/
│   ├── state_reader.hpp / state_reader.cpp
│   ├── command_writer.hpp / command_writer.cpp
│   ├── calculation_writer.hpp / calculation_writer.cpp
│   ├── console_log.hpp / console_log.cpp
│   ├── connection.hpp / tcp_connection.cpp
│   └── messages.hpp
├── common/
│   ├── util_functions.hpp
│   └── roles.hpp
└── third_party/nlohmann/
```

## Hangi dosyaya gidilir?

| İş | Kaynak |
|---|---|
| Fighter'ın aşama ve atış kararları | [control/otopilot_fighter.cpp](control/otopilot_fighter.cpp) |
| Hostile'in karşı senaryosu | [control/otopilot_hostile.cpp](control/otopilot_hostile.cpp) |
| Adımın okuma–hesap–karar–yazma sırası | [control/sim.cpp](control/sim.cpp) |
| Manevranın yön hedefi üretmesi | [control/maneuver.cpp](control/maneuver.cpp) |
| Ortak ölçüm ve füze verileri | [model/aircraft.hpp](model/aircraft.hpp) |
| Hesapları ilgili uçak/füze alanlarına koymak | [model/world.cpp](model/world.cpp) |
| Uçak sabitleri ve slot sırasındaki mühimmat tipleri | [model/aircraft_profiles.hpp](model/aircraft_profiles.hpp) |
| Füze menzil, hız ve pitbull parametreleri | [model/missile_profiles.hpp](model/missile_profiles.hpp) |
| Mesafe, bearing, ATA, aspect, LOS ve kapanma | [calculations/geometry.cpp](calculations/geometry.cpp) |
| WEZ, MAR ve DOR | [calculations/engagement.cpp](calculations/engagement.cpp) |
| Füzenin hedefine göre hesapları | [calculations/missile_geometry.cpp](calculations/missile_geometry.cpp) |
| JSON ölçümünü C++ nesnelerine okumak | [io/state_reader.cpp](io/state_reader.cpp) |
| Uçuş ve atış komutlarını JSON'a çevirmek | [io/command_writer.cpp](io/command_writer.cpp) |
| C++ hesabını kayıt/görüntüleyiciye çıkarmak | [io/calculation_writer.cpp](io/calculation_writer.cpp) |
| TCP çerçeveleme ve adım sırası | [io/tcp_connection.cpp](io/tcp_connection.cpp) |
| Açı/birim ve koordinat yardımcıları | [common/util_functions.hpp](common/util_functions.hpp) |

## Derleme

Proje kökünden:

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles"
cmake --build src/otopilot/build
ctest --test-dir src/otopilot/build --output-on-failure
```

CMake C++17 ve Windows Winsock kullanır. Çıktı `src/otopilot/main.exe` dosyasıdır. Proje içi include yolları köke göredir: `#include "model/world.hpp"` gibi. `.hpp` ve `.cpp` aynı görev klasöründe tutulur.

Kurulum, Python'u önce başlatma ve adres ayarları [çalıştırma kılavuzundadır](../../docs/CALISTIRMA_KILAVUZU.md).

## Birim sözleşmesi

| Büyüklük | C++ birimi / alan örneği |
|---|---|
| İrtifa | ft — `altitude_ft` |
| Mesafe ve menzil | NM — `range_3d_nm`, `mar_nm`, `radar_range_nm` |
| Hız ve kapanma | kt — `tas_kt`, `rate_of_closure_kt` |
| Zaman / açı | saniye / derece — `_s`, `_deg` |

`tas_kt` gerçek hava hızıdır (KTAS); IAS/CAS hesabı yapılmaz. LOS konum
vektörünün üç bileşeni NM, NED hız vektörünün üç bileşeni kt'dir.
`los_unit`, Mach ve yük faktörü birimsizdir. Patlama yarıçapı feet'tir;
MAR toplamına eklenmeden önce NM'ye çevrilir.

Python'un uçuş mesajları metre/m/s kullanır. `StateReader` ölçüm ve mevcut
komutları C++ birimlerine, `CommandWriter` irtifa hedefini tekrar metreye
çevirir. `CalculationWriter` ise hesapları `_nm` ve `_kt` anahtarlarıyla
doğrudan kayda çıkarır. Yeni nesne CSV'leri `alt_ft`, `tas_kt`,
`vel_north/east/down_kt`, `cmd_alt_ft` alanlarını kullanır.

NM / kt saat verdiği için saniye hesaplarında `3600` çarpanı gerekir.
Dönüş yarıçapında hız ft/s, yerçekimi ft/s² olmalıdır; sonuç NM'ye çevrilir.
Dönüşüm yardımcıları `common/util_functions.hpp` içindedir.

## Geliştirme rehberleri

- [Otopilot mimarisi](../../docs/mimari/OTOPILOT_MIMARISI.md): nesne sahipliği, hesap sırası, füze yaşam döngüsü ve model sınırları.
- [Kullanım örnekleri](KULLANIM.md): aşama, manevra, profil ve kayıt alanı ekleme.
- [Veri referansı](../../docs/veri_info/SIMULASYON_VERI_REFERANSI.md): JSON/CSV alanları, birimler ve kimlikler.

Uçuş hedefleri `FlightCommand` içinde sade veri olarak kalır. JSON ayrıntıları `io` katmanındadır. Taktik durum her uçağın kendi otopilotunda, ortak ölçüm ve hesaplar `World` içindedir.
