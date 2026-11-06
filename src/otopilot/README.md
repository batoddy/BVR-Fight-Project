# C++ otopilot kaynakları

C++ programı Python simülasyonundan ölçüm alır, ortak hesapları günceller ve iki uçağın komutunu üretir. Giriş `main.cpp`, adımın yöneticisi `Sim`, uçak davranışlarının sahipleri `OtopilotFighter` ile `OtopilotHostile` sınıflarıdır (her biri tek switch'li bir durum makinesi). Kod kuralları: [KOD_STANDARTLARI.md](KOD_STANDARTLARI.md).

```text
otopilot/
├── main.cpp
├── CMakeLists.txt
├── KOD_STANDARTLARI.md
├── control/
│   ├── sim.hpp / sim.cpp
│   ├── otopilot_fighter.hpp / otopilot_fighter.cpp
│   ├── otopilot_hostile.hpp / otopilot_hostile.cpp
│   ├── otopilot_state.hpp                 (iki otopilotun ortak durum listesi)
│   ├── maneuver.hpp / maneuver.cpp
│   ├── tactic.hpp / tactic.cpp            (taktikler: manevraların kurallı sırası; henüz bağlı değil)
│   └── conditions.hpp / conditions.cpp    (geçiş koşulları)
├── model/
│   ├── aircraft.hpp
│   ├── weapons.hpp / weapons.cpp          (füze slotları, atış, WEZ/NEZ/F-pole)
│   ├── world.hpp / world.cpp
│   ├── picture.hpp                        (hesaplanan durum resmi)
│   ├── turn_result.hpp                    (dönüş hesabının sonuç tipi)
│   ├── tactic_id.hpp
│   ├── kinematics.hpp
│   ├── missile.hpp
│   ├── flight_command.hpp
│   ├── phase.hpp
│   ├── aircraft_profiles.hpp
│   └── missile_profiles.hpp
├── calculations/
│   ├── geometry.hpp / geometry.cpp
│   ├── turn.hpp / turn.cpp
│   ├── engagement.hpp / engagement.cpp
│   ├── missile_geometry.hpp / missile_geometry.cpp
│   ├── r_cold_formulu.md, r_cold_grafikler/      (R_cold uyumu ve ölçümü)
│   ├── wez_grafikler/ (wez_sonuc.txt)            (WEZ dış sınırı Rmax uyumu ve ölçümü)
│   └── fuze_ucus_tablosu.md                      (füze uçuş modeli)
├── io/
│   ├── state_reader.hpp / state_reader.cpp
│   ├── command_writer.hpp / command_writer.cpp
│   ├── calculation_writer.hpp / calculation_writer.cpp
│   ├── console_log.hpp / console_log.cpp
│   ├── connection.hpp / tcp_connection.cpp
│   └── messages.hpp
├── common/
│   ├── units.hpp / angles.hpp / ned.hpp / formulas.hpp
│   └── roles.hpp
├── tests/
│   └── units_test.cpp
└── third_party/nlohmann/
```

## Hangi dosyaya gidilir?

| İş | Kaynak |
|---|---|
| Fighter'ın aşama ve atış kararları | [control/otopilot_fighter.cpp](control/otopilot_fighter.cpp) |
| Hostile'in karşı senaryosu | [control/otopilot_hostile.cpp](control/otopilot_hostile.cpp) |
| Geçiş koşulları | [control/conditions.cpp](control/conditions.cpp) |
| Adımın okuma–hesap–karar–yazma sırası | [control/sim.cpp](control/sim.cpp) |
| Manevraların komut üretmesi ve bitişi | [control/maneuver.cpp](control/maneuver.cpp) |
| Ortak ölçüm ve füze verileri | [model/aircraft.hpp](model/aircraft.hpp) |
| Hesaplanan durum: geometri, kaçış, tehdit | [model/picture.hpp](model/picture.hpp) |
| Füze slotları, atış, kendi WEZ/NEZ/F-pole | [model/weapons.hpp](model/weapons.hpp) |
| Hesapları ilgili uçak/füze alanlarına koymak | [model/world.cpp](model/world.cpp) |
| Uçak sabitleri ve slot sırasındaki mühimmat tipleri | [model/aircraft_profiles.hpp](model/aircraft_profiles.hpp) |
| Füze menzil, hız, pitbull, R_cold ve Rmax (WEZ) parametreleri | [model/missile_profiles.hpp](model/missile_profiles.hpp) |
| Mesafe, bearing, ATA, aspect, LOS ve kapanma | [calculations/geometry.cpp](calculations/geometry.cpp) |
| Dönüş süresi, yarıçapı, yer değişimi | [calculations/turn.cpp](calculations/turn.cpp) |
| WEZ, A/F-pole, TR, out planı, R_cold, NEZ, MAR, MRR, DR, DOR | [calculations/engagement.cpp](calculations/engagement.cpp) |
| Füzenin hedefine göre hesapları ve uçuş uyumu | [calculations/missile_geometry.cpp](calculations/missile_geometry.cpp) |
| JSON ölçümünü C++ nesnelerine okumak | [io/state_reader.cpp](io/state_reader.cpp) |
| Uçuş ve atış komutlarını JSON'a çevirmek | [io/command_writer.cpp](io/command_writer.cpp) |
| C++ hesabını kayıt/görüntüleyiciye çıkarmak | [io/calculation_writer.cpp](io/calculation_writer.cpp) |
| TCP çerçeveleme ve adım sırası | [io/tcp_connection.cpp](io/tcp_connection.cpp) |
| Birim, açı, NED ve hareket formülü yardımcıları | [common/](common/) |

## Derleme

Proje kökünden:

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles"
cmake --build src/otopilot/build
ctest --test-dir src/otopilot/build --output-on-failure
```

CMake C++17 ve Windows Winsock kullanır. Çıktı `src/otopilot/main.exe` dosyasıdır. Proje içi include yolları köke göredir: `#include "model/world.hpp"` gibi. `.hpp` ve `.cpp` aynı görev klasöründe tutulur.

Kurulum, Python'u önce başlatma ve adres ayarları [çalıştırma kılavuzundadır](../../docs/CALISTIRMA_KILAVUZU.md).

Genel manevra denemesi: Python'u `--duration 1200` ile, C++'ı `main.exe --maneuver-test` ile çalıştır.
18 zamanlı adımın sırası ve süreleri `control/otopilot_maneuver_test.cpp` içindedir; [test kullanımını](KULLANIM.md#otopilotla-genel-manevra-testi) incele.

## Birim sözleşmesi

| Büyüklük | C++ birimi / alan örneği |
|---|---|
| İrtifa | ft — `altitude_ft` |
| Mesafe ve menzil | NM — `range_3d_nm`, `mar_nm`, `radar_range_nm` |
| Hız ve kapanma | kt — `tas_kt`, `closure_kt` (kayıtta `rate_of_closure_kt`) |
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
Dönüş yarıçapı m/s ve m/s² ile hesaplanır (`turn::radius_m`); yer değişimi NM'ye çevrilir.
Dönüşüm yardımcıları `common/units.hpp` içindedir.

## Geliştirme rehberleri

- [Otopilot mimarisi](../../docs/mimari/OTOPILOT_MIMARISI.md): nesne sahipliği, hesap sırası, füze yaşam döngüsü ve model sınırları.
- [Kullanım örnekleri](KULLANIM.md): aşama, manevra, profil ve kayıt alanı ekleme.
- [Veri referansı](../../docs/veri_info/SIMULASYON_VERI_REFERANSI.md): JSON/CSV alanları, birimler ve kimlikler.

Uçuş hedefleri `own.command` (`FlightCommand`) içinde setter'larla yazılır. JSON ayrıntıları `io` katmanındadır. Taktik durum her uçağın kendi otopilotunda, ortak ölçüm ve hesaplar `World` içindedir.
