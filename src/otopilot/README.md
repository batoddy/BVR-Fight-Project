# C++ TCP istemcisi — otopilot

Python simülasyonu ile tek TCP bağlantısında ölçüm/komut alışverişi yapar.
Gelen ölçüm nesnelere doldurulur; kendi karar kodun `controller.cpp` içine gelir.

## Dosyalar

| Dosya | Görevi |
|---|---|
| `main.cpp` | Ölçümü al → hesabı çağır → komutu ver; TCP ayrıntısı içermez. |
| `controller.cpp/.hpp` | Uçuş hedeflerini ve atış emirlerini belirleyen, senin değiştireceğin sınıf. |
| `objects.hpp/.cpp` | `World`, `Aircraft`, `Missile`, `Kinematics` (ölçüm) ve `FlightCommand` (uçuş ve atış komutu). JSON ↔ nesne çevirisi burada. |
| `aircraft_profiles.hpp` | Type adı → uçak özellikleri lookup table. |
| `angles.hpp` | `PI`, derece/radyan, 0..360 ↔ ±180 yön çevirisi. |
| `geometry.hpp/.cpp` | WGS84 → ECEF, range, LOS (kuzey/doğu/aşağı), yatay mesafe. Formüller `.cpp` içinde yorum olarak. |
| `connection.hpp` | Uygulamaya sunulan sade `Connection` al/ver arayüzü. |
| `tcp_connection.cpp` | Adres/port, Winsock, JSON aktarımı, adım sırası ve bitiş/kapanış. |
| `messages.hpp` | Python ile aynı `kind`, `tick`, `data` alanları. |
| `third_party/nlohmann` | JSON okuma/yazma kütüphanesi 3.12.0 ve MIT lisansı. |

## Derle ve çalıştır

Workspace içinde PowerShell'den, kurulu MinGW ile:

```powershell
& "M:\bin\g++.exe" -no-canonical-prefixes -std=c++17 -Wall -Wextra -O2 -static ".\src\otopilot\*.cpp" -o ".\src\otopilot\main.exe" -lws2_32
```

`*.cpp` klasördeki bütün `.cpp` dosyalarını birlikte derler (MinGW joker
karakteri kendisi açar); yeni bir `.cpp` eklediğinde komut değişmez. `.hpp`
dosyaları include ile alınır. Düzen: `.hpp` = tipler ve bildirimler, `.cpp` =
hesap kodu; sabitler (`angles.hpp`) ve veri tablosu (`aircraft_profiles.hpp`)
başlıkta kalır. `-lws2_32` Windows TCP kütüphanesini bağlar.
`-static` MinGW çalışma zamanını exe içine alır.

1. Python terminalinde venv açıkken sunucuyu başlat: `python .\src\sim\sim_main.py`
2. İkinci terminalde C++ istemcisini başlat: `.\src\otopilot\main.exe`

Python'u önce başlat; C++ önce açılırsa "Once sim_main.py'yi baslat" diyerek durur.
Varsayılan adres `127.0.0.1:6000`. Port **iki yerde** durur: Python
`src/sim/scenario.yaml` içindeki `connection.port`'u, C++ ise bu varsayılanı
kullanır. YAML'da değiştirirsen C++'a da ver: `.\src\otopilot\main.exe 127.0.0.1 6001`.

Süre tamamlanınca Python `end` gönderir; C++ normal şekilde kapanır. Komut
beklerken Python fiziği ilerlemez. Bekleme zaman aşımı yoktur; taraflardan biri
takılırsa diğeri bekler.

`M:` eşlemesi kaybolmuşsa (derleyici yolundaki boşluk linker'ı bozar):

```powershell
subst M: "$env:USERPROFILE\tools\mingw64"
```

## Kendi uçuş kodun

`Controller::calculate()` her turda gelen ölçümü nesnelere doldurur:

```cpp
World world = World::from_json(state.data);
```

`blue` her zaman **fighter**, `red` her zaman **bogey**:

```cpp
world.sim_time_s
world.fighter.kinematics.alt_msl_m      // m
world.fighter.kinematics.yaw_deg        // ±180, aşağıya bak
world.bogey.kinematics.lat_deg          // jeodezik enlem (WGS84)
world.bogey.health                      // 1 sağlam, isabet alınca düşer
world.fighter.profile.max_mach          // aircraft_profiles.hpp

for (const Missile& m : world.bogey_missiles)
    if (m.kinematics)                   // yalnızca atılmış füzede var
        m.kinematics->alt_msl_m;
```

Füzenin `kinematics` ve `flight_time_s` alanları atılana kadar **yok**
(`std::optional`); `if (m.kinematics)` ile bak. `target_id` ve `outcome` yoksa
boş string. `m.state`: `"ready"`, `"in_flight"`, `"ended"`.

Hedefler `fighter_command_` ve `bogey_command_` içinde durur. İlk ölçümde
YAML'daki başlangıç hedefleriyle dolar; değiştirmezsen sonraki turlarda aynı
kalır. Python her uçak için komut istediği için bogey'in komutu da gönderilir.

```cpp
fighter_command_.heading_deg = -90.0;   // batı
fighter_command_.altitude_m = 9000.0;   // m
fighter_command_.throttle = 0.6;        // 0..1
```

### Yön: C++ içinde ±180

Python/JSBSim yönü 0..360 kullanır; C++ içinde **±180** kullanıyoruz:

| Yön | Python | C++ |
|---|---|---|
| kuzey | 0 | 0 |
| doğu | 90 | +90 (sağ) |
| güney | 180 | 180 |
| batı | 270 | −90 (sol) |

Çeviri yalnızca `objects.cpp` içinde yapılır: okurken `yaw_deg` ve başlangıç
`heading_deg` ±180'e, komut gönderirken `heading_deg` 0..360'a. Hesap kodunda
hep ±180 görür ve yazarsın. `-200` veya `400` gibi taşan değerler de doğru yöne
çevrilir. Roll ve pitch zaten işaretli geldiği için dokunulmaz.

### Füze atışı

Atış, uçağın komut nesnesi üzerinden verilir:

```cpp
fighter_command_.fire(world.bogey);     // blue, red'e ateş etsin
bogey_command_.fire(world.fighter);     // red, blue'ya ateş etsin
```

Yön/irtifa/gaz kalıcıdır, atış **tek seferliktir**: controller emri gönderdikten
sonra `fire_target_id`'yi temizler. Atış o tur mümkün değilse emir beklemez,
düşer; "imkân olunca at" istiyorsan `fire()`'ı her tur çağır.

Python `simulation.fire()` ile uygular. Havada kendi füzesi varsa veya hazır
füze kalmadıysa atış yapılmaz; bunu bir sonraki ölçümde füzenin `state`'inden
(`"ready"` → `"in_flight"`) görürsün. Atış ve sonucu `events.jsonl`'a kaydedilir.

### Korumalar

- Komutta `NaN` veya sonsuz değer varsa C++ göndermeden durur ve hangi uçak
  olduğunu yazar. (nlohmann `NaN`'ı `null` yazar; Python sebebini söyleyemezdi.)
- Tip `aircraft_profiles.hpp` tablosunda yoksa ilk ölçümde
  `Bilinmeyen ucak tipi: ...` hatasıyla durur. Yeni tip için tabloya bir satır ekle.

## Lookup table

Tablo `aircraft_profiles.hpp` içindedir. Anahtar YAML'daki `type` ile birebir
aynıdır: `F16`, `Su57`, `CustomAircraft`. Yeni özellik eklerken alanı
`AircraftProfile` struct'ına, değerini tabloya yaz; `world.fighter.profile.<alan>`
ile erişirsin. Mevcut alanlar: `max_mach`, `max_g`, `wez_max_range_m`,
`wez_min_range_m`, `radar_range_m`, `radar_gimbal_deg`, `turn_range_m`.
Tablodaki değerler struct'taki alan sırasıyla yazılır; başlıktaki yorum satırı
sütunları gösterir. F16'nın Mach, g ve radar değerleri açık kaynaktandır; WEZ
gerçek AIM-120 değeri değil, BVRGym füze modelinde ölçülmüştür; `turn_range_m`
örnektir. Su57 ve CustomAircraft satırları tamamen örnektir. Kaynaklar ve ölçüm
koşulları dosyanın içinde, tablonun üstünde yazılıdır.

## Veri nasıl taşınıyor?

`Message` nesnesi → tek satır UTF-8 JSON + `\n` → TCP → karşı taraftaki Message.
Satırlar en fazla 1 MiB. Gönderme eksik kalırsa kalan baytlar gönderilir; okuma
mesaj parçalanınca tamamını bekler, birden fazla mesaj gelince sonraki baytları saklar.
Adım sırası ve mesaj türü kontrol edilir. Otomatik yeniden bağlanma yoktur.

Komut mesajı:

```json
{"kind": "command", "tick": 24, "data": {
  "aircraft": [{"id": "blue", "heading_deg": 270, "altitude_m": 8000, "throttle": 0.49},
               {"id": "red",  "heading_deg": 180, "altitude_m": 3000, "throttle": 0.49}],
  "launch":   [{"id": "blue", "target_id": "red"}]
}}
```

`main.cpp` adres/port, socket, JSON çerçeveleme, tick sayacı veya end mesajı
işlemez; bunlar `tcp_connection.cpp` içindedir.

## Doğrulama

12 Ekim 2026: Gerçek MinGW ile derlenen istemci, gerçek Python/BVRGym sunucusuyla 180 simülasyon saniyesini tamamladı (2161 komut, 2162 kayıt). Parçalı/birleşik mesaj, Unicode, yanlış tick, yarım mesaj/kopma ve boyut sınırı kontrol edildi.

12 Ekim 2026, nesneler / ±180 / atış: C++ birim testi 24/24 geçti (yön dönüşümleri 0..360 aralığında 0,5° adımla gidiş-dönüş, ölçümden nesne, füze `optional`, NaN komutun durdurulması, bilinmeyen tip hatası). Değişmemiş controller ile gerçek uçuş: 2161 komut, 2162 kayıt. Ayrı bir test controller'ı ile: 2,09 s'de `blue → red` atışı `missile_launched` olarak kaydedildi, 62,8 s'de `missile_lost`; C++'ın yazdığı `-90` Python'a `270` gitti ve uçak 0° → 338° → 305° → 275° ile sola döndü.
