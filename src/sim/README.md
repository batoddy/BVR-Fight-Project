# Python simülasyon dosyaları

Güncelleme: 12 Ekim 2026. C++ ve Python ayrı programlar olarak çalışır.
Python ölçümü gönderir, C++ komutunu bekler ve ardından bir adım ilerler.

## Hangi dosya ne yapar?

| Dosya | Görevi |
|---|---|
| `sim_main.py` | YAML'ı okur; simülasyon, TCP ve recorder'ı sırayla çağırır. |
| `simulation.py` | Config kopyalar, ajan oluşturur, ölçüm okur ve komut uygular. |
| `connector.py` | `Message` ve `TCPConnection`: yalnızca JSON mesajı gönder/al. |
| `recorder.py` | Mevcut CSV, events.jsonl ve manifest.json kaydını alır. |
| `sim_local.py` | C++ olmadan, YAML'daki ilk hedeflerle yerel uçuş yapar. |

`Simulation` oluşturulunca YAML'daki bütün uçaklar ve mühimmatları oluşturulur.
`read_state()` sadece okur. `step(aircraft_commands)` bütün uçakları ilerletir;
TCP, kayıt ve uyuma bu sınıfta bulunmaz. Böylece kendi döngünde de kullanabilirsin.
Import yapmak simülasyonu veya sunucuyu başlatmaz; giriş dosyası `main()` çağırır.

## Çalıştırma

Workspace içinde mevcut venv'i kullan:

```powershell
# Python sunucusunu aç; ardından C++ istemcini çalıştır.
python .\src\sim\sim_main.py

# C++ kullanmadan yerel uçuşu çalıştırmak için:
python .\src\sim\sim_local.py
```

`src/sim/scenario.yaml` içindeki `connection.host` ve `connection.port` TCP adresidir.
Varsayılan `127.0.0.1:6000`. `world.duration_s`, `sim_time_step`, `sleep_s`
ayarları aynı YAML'dan alınır. `sleep_s` yalnızca dış döngünün sonunda uygulanır.
C++ hesabı ve iletişim süresi de gerçek sürenin parçasıdır.

## Ölçüm → komut → ilerletme

1. `sim_main` ölçümü `Simulation.read_state()` ile okur ve gönderir.
2. `connection.receive()` C++ komutunun tamamlanmasını bekler. Uçaklar bu sırada ilerlemez.
3. Komutun `kind` alanı `command`, `tick` alanı mevcut adımla aynı olmalı.
4. `Simulation.step()` her uçak için bir yön/irtifa/gaz komutu ister. Eksik,
   tekrarlanan, bilinmeyen veya geçersiz komut varsa hiçbir uçak ilerlemez.
5. Recorder yeni durumu kaydeder. Süre dolunca `end` mesajı gönderilir.

Bir bağlantı tekrar tekrar kullanılır. Her mesaj UTF-8, tek satır JSON ve `\n`
ile biter. TCP mesajı parçalasa veya iki mesaj birlikte gelse de satırlar ayrı okunur.
En büyük mesaj satır sonu dahil 1 MiB. Bağlantı kapanırsa döngü durur ve kayıtlar kapanır;
yeniden bağlanma otomatik yapılmaz. İstemci komut göndermiyorsa beklemeye devam edilir.

## Mesajın üç alanı

| Alan | Anlamı |
|---|---|
| `kind` | `state`, `command` veya `end`. |
| `tick` | Fizik saniyesi değil, dış ilerletme adımının sayacı. |
| `data` | Ölçümde bütün uçak/füzeler; komutta uçak hedeflerinin listesi. |

Ölçümde `data.sim_time_s` gerçek JSBSim saatidir; başlangıç resetinden dolayı
ilk ölçüm sıfırdan biraz büyük olabilir. `data.aircraft` uçakları, `data.missiles`
mühimmat durumunu içerir. Atılmamış füzenin `kinematics` ve `flight_time_s`
alanları `null`; hazır/havada/sona erdi durumları `ready/in_flight/ended` olarak gelir.
Uçakların açısı derece, hızı m/s, irtifası `position/h-sl-meters` kaynağından metredir.
Jeodezik enlem `position/lat-geod-deg` üzerinden okunur; BVRGym spawn enlemini
jeosantrik olarak kullanır. Recorder'ın kendi ham sütun ve referansları değiştirilmedi.

Komut örneği (tek satır gönderilir):

```json
{"kind":"command","tick":0,"data":{"aircraft":[{"id":"blue","heading_deg":90,"altitude_m":5000,"throttle":0.49},{"id":"red","heading_deg":270,"altitude_m":5000,"throttle":0.49}]}}
```

JSON şablonları: `src/msgs/state.json`, `src/msgs/command.json`, `src/msgs/end.json` (bu dosyadan göreli klasör: `../msgs/`).
Dosyalar aktarımda okunmaz/yazılmaz; mesajlar TCP'de bellekte aktarılır.

## Uçuş kodunu değiştirmek

`simulation.agents["blue"]` BVRGym'in `BaseBVRAgent` nesnesidir.
Yerel uçuşta `sim_local.py` içindeki komut listesini değiştirebilirsin.
TCP kullanımında C++ her döngüde bütün uçakların hedeflerini doldurur.
Otomatik hedef seçimi, manevra veya atış eklenmedi.

`simulation.fire("blue", "red")` açık Python atış çağrısıdır; atış uygulanırsa
`True`, aynı uçak/hedef, mühimmat yokluğu veya aktif füze kısıtında `False` döner.
Şimdiki TCP komut mesajı yalnızca uçuş hedeflerini uygular. Füze atış isteğini
mesaja eklemek sonraki genişleme; envanter ve uçan füze ölçümleri şimdiden yayınlanır.

C++ istemci `src/otopilot` içinde uygulanmıştır. Derleme ve iki terminalden kullanım
için `../otopilot/README.md` dosyasına bak. Python tarafına yeni paket gerekmez.
Ölçümdeki `data.flight_commands` son uygulanan/saklanan yön, irtifa ve gaz hedefleridir;
C++ ilk ölçümde buradan YAML başlangıç hedeflerini alır.



## Paket ve dosya yolları

Kodlar `src/sim`, JSON mesaj şablonları `src/msgs` içindedir. Senaryo
`src/sim/scenario.yaml` dosyasından okunur. BVRGym ve kayıt klasörleri
Workspace altında kalır; yollar çalıştırılan terminalin konumuna göre değişmez.

Workspace içinden doğrudan dosya komutlarının yanında şunlar da kullanılabilir:

```powershell
python -m src.sim.sim_local
python -m src.sim.sim_main
```

Dosya doğrudan çalıştırıldığında komşu modüller, paket olarak çalıştırıldığında
göreli paket import'ları kullanılır. JSON şablonları örnek değerler taşır;
çalışan simülasyon bu dosyaları okuyup komut uygulamaz veya ölçüm yazmaz.


## Uçak tipi

Her ajan için `src/sim/scenario.yaml` içinde `type` yazılabilir:

```yaml
agents:
  blue:
    name: KE
    type: F16
  red:
    name: Bogey
    type: Su57
```

Bu parça alanı gösterir; mevcut takım/mühimmat/spawn ayarlarını koru.
`data.aircraft` içindeki her nesnenin `type` alanı C++'a TCP üzerinden taşınır.
Alan serbest bir metindir: `F16`, `Su57`, `CustomAircraft` gibi profil adları
kullanılabilir. Büyük/küçük harf korunur, baştaki/sondaki boşluklar temizlenir.
Alan yoksa eski senaryolar için `F16` kullanılır; boş veya metin olmayan değer reddedilir.

Type, ileride WEZ gibi hesaplarda seçilecek profil etiketidir. Bu alan tek başına
JSBSim fizik modelini değiştirmez; uçaklar mevcut F-16 modeliyle simüle edilir.
Yeni fizik modeli için XML/config adaptasyonu ayrı yapılmalıdır. WEZ hesabı eklenmedi.
Senaryo type alanı recorder'ın manifest içindeki senaryo kopyasında da saklanır.
