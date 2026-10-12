# C++ TCP istemcisi — otopilot

Python simülasyonu ile tek TCP bağlantısında ölçüm/komut alışverişi yapar.
İlk sürüm YAML'daki uçuş hedeflerini korur; kendi karar kodun `controller.cpp` içine gelir.

## Dosyalar

| Dosya | Görevi |
|---|---|
| `main.cpp` | Ölçümü al → hesabı çağır → komutu ver; TCP ayrıntısı içermez. |
| `controller.cpp/.hpp` | Uçuş hedeflerini belirleyen, senin değiştireceğin sınıf. |
| `connection.hpp` | Uygulamaya sunulan sade `Connection` al/ver arayüzü. |
| `tcp_connection.cpp` | Adres/port, Winsock, JSON aktarımı, adım sırası ve bitiş/kapanış. |
| `messages.hpp` | Python ile aynı `kind`, `tick`, `data` alanları. |
| `third_party/nlohmann` | JSON okuma/yazma kütüphanesi 3.12.0 ve MIT lisansı. |

Asio veya gömülü Python kullanılmaz. C++ mesajları bellekte JSON'a çevirir;
`src/msgs` içindeki JSON şablonlarını çalışma sırasında okumaz.

## Derle ve çalıştır

Workspace içinde PowerShell'den, kurulu MinGW ile:

```powershell
& "M:\bin\g++.exe" -no-canonical-prefixes -std=c++17 -Wall -Wextra -O2 -static ".\src\otopilot\main.cpp" ".\src\otopilot\controller.cpp" ".\src\otopilot\tcp_connection.cpp" -o ".\src\otopilot\main.exe" -lws2_32
```

Üç `.cpp` birlikte derlenir; `.hpp` dosyaları include ile alınır.
`-lws2_32` Windows TCP kütüphanesini bağlar. `-static` MinGW çalışma zamanı
bağımlılıklarını exe içine alır. C++ kaynaklarını değiştirince yeniden derle.

1. Python terminalinde venv açıkken sunucuyu başlat:

```powershell
python .\src\sim\sim_main.py
```

2. İkinci terminalde C++ istemcisini başlat:

```powershell
.\src\otopilot\main.exe
```

Varsayılan adres `127.0.0.1:6000`. Python YAML'ında adres/portu değiştirdiysen:

```powershell
.\src\otopilot\main.exe 127.0.0.1 6001
```

Python'u önce başlat; bağlantı hatasında C++ çıkış kodu 1 ile durur.
Süre tamamlanınca Python `end` gönderir; C++ normal şekilde kapanır.
Komut beklerken Python fiziği ilerlemez. Bağlantı kopunca Python kaydı kapatır.
TCP beklerken Ctrl+C davranışı kullanıcının isteğiyle değiştirilmedi.

`M:` eşlemesi kaybolmuşsa mevcut derleyicinin eşlemesini tekrar kur:

```powershell
subst M: "$env:USERPROFILE\tools\mingw64"
```

## Kendi uçuş kodun

`Message state` Python'dan gelen ölçümdür. Alanlarını `state.data` üzerinden okursun:

```cpp
for (const auto& aircraft : state.data.at("aircraft")) {
    std::string id = aircraft.at("id").get<std::string>();
    double altitude = aircraft.at("alt_msl_m").get<double>();
    double speed = aircraft.at("tas_mps").get<double>();
    // Kendi hesabın burada yapılabilir.
}
```

`Controller::calculate()` ilk ölçümde `flight_commands` alanından YAML'daki
hedefleri alır ve `targets_` içinde saklar. Her döngüde bunlardan aynı tick'e
ait `command` mesajını oluşturur. Mevcut ölçümden her seferinde yeni hedef
üretmediği için belirlediğin hedef sonraki döngüde de korunur.

`controller.cpp` içindeki yorum yerine şu örneği açarsan blue'nun hedef yönü değişir:

```cpp
for (auto& target : targets_)
    if (target.at("id") == "blue") target["heading_deg"] = 90.0;
```

İrtifa `altitude_m` (metre), yön `heading_deg` (0..360 derece), gaz `throttle`
(0..1). `state.data.at("missiles")` mühimmat/füze durumlarını taşır; hazır füzenin
kinematik alanları `null`. TCP üzerinden füze ateşleme bu sürümde eklenmedi.

## Veri nasıl taşınıyor?

`Message` nesnesi → tek satır UTF-8 JSON + `\n` → TCP → karşı taraftaki Message.
Satırlar en fazla 1 MiB. Gönderme eksik kalırsa kalan baytlar gönderilir; okuma
mesaj parçalanınca tamamını bekler, birden fazla mesaj gelince sonraki baytları saklar.
Adım sırası ve mesaj türü kontrol edilir. Otomatik yeniden bağlanma yoktur.

## Doğrulama

12 Ekim 2026: Gerçek MinGW ile derlenen istemci, gerçek Python/BVRGym sunucusuyla 180 simülasyon saniyesini tamamladı (2161 komut, 2162 kayıt). Başlangıç uçuş hedeflerinin CSV'ye doğru yazılması, parçalı/birleşik mesaj, Unicode, yanlış tick, yarım mesaj/kopma ve boyut sınırı kontrol edildi. Test kayıtları geçici klasörde tutuldu.



## Sade ana döngü

`main.cpp` adres/port, socket, JSON çerçeveleme, tick sayacı veya end mesajı işlemez.
`Connection(argc, argv)` başlangıç ayarlarını kendi uygulamasında çözer.
`receive(state)` nesneyi doldurur; simülasyon bitince `false` döndürür.
`send(command)` mesaj türü ve adımı doğrulayıp gönderir. Bu kontroller
`tcp_connection.cpp` içindedir; Windows başlıkları da yalnızca orada bulunur.

```cpp
while (connection.receive(state)) {
    Message command = controller.calculate(state);
    connection.send(command);
}
```

Taşıma ayrıntıları `Connection::Impl` içinde saklanır. Main yalnızca genel
mesaj arayüzünü kullanır. Buradaki al/ver işlemi TCP üzerinden bellekteki
mesaj aktarımıdır; JSON şablonları diskten okunmaz/yazılmaz. Derleme ve terminal
komutları aynı kaldı.
