> **12 Ekim 2026 güncellemesi:** Çalışan Python TCP köprüsü bütün uçakları tek ölçüm/komut içinde taşır. Güncel şablonlar [tcp/](tcp/README.md), kullanım [src/README.md](../../../src/README.md). Aşağıdaki sürüm 2, uçak başına mesaj önerisinin eski tasarım notudur; mevcut bağlantının şeması değildir. C++ istemci henüz projeye eklenmedi.

# Simülasyon mesaj şablonları — sürüm 2

**Her durum ve komut mesajı tek bir uçağa aittir.** Mavi ve kırmızı aynı şemayı kullanır; kimlikleri ve verileri farklıdır. Böylece her uçağı ayrı C++ sınıfı, fonksiyonu veya kod bloğu kontrol edebilir.

Publisher veya C++–Python bağlantısı henüz uygulanmadı. JSON dosyaları veri sözleşmesinin temsili örnekleridir; gerçek bir uçuş veya füze performans kaydı değildir. Simülasyon kodu değişmedi.

## Dosyalar

| Dosya | Kullanım |
|---|---|
| [telemetry_blue.json](telemetry_blue.json) | Yalnızca blue uçağı ve blue'ya ait mühimmat |
| [telemetry_red.json](telemetry_red.json) | Yalnızca red uçağı ve red'e ait mühimmat |
| [command_blue.json](command_blue.json) | Yalnızca blue uçağına uçuş/atış komutu |
| [command_red.json](command_red.json) | Yalnızca red uçağına uçuş komutu; events boş |
| [missile_state_examples.json](missile_state_examples.json) | Tek bir füzenin hazır, rezerveli, uçuşta ve bitmiş durum örnekleri |
| `archive/v1/` | Önceki birleşik mesajlar ve açıklamaları; güncel sözleşme değildir |

Eski aircraft dizisi tek `aircraft` nesnesi oldu. Eski çok ajanlı `actions` nesnesi tek `action` nesnesi oldu. Yapı uyumsuz değiştiği için `schema_version` 2'ye çıkarıldı. Füze durum örneklerinin iç yapısı değişmedi.

## Tek uçak durum mesajı

| Alan | Tip | Anlam |
|---|---|---|
| `schema_version` | int | 2 |
| `type` | string | `aircraft_state` |
| `session_id` | string | Her çalıştırmada yeni, benzersiz oturum kimliği |
| `aircraft_id` | string | Mesajın sahibi: YAML ajan anahtarı, ör. blue/red |
| `packet_no` | int | Bu uçak için oturum içinde artan yayın numarası |
| `tick` | int | Ortak simülasyon dış döngü adımının numarası |
| `sim_time_s` | number | Ortak senaryo zamanı; simülasyon saniyesi |
| `aircraft` | object | Yalnızca aircraft_id ile eşleşen uçağın durumu |
| `missiles` | array | Yalnızca owner_id = aircraft_id olan mühimmat |

Uçak kimliği sabittir; görünen isimden veya takım renginden türetilmez. `aircraft.id` ve üstteki `aircraft_id` aynı olmalıdır. Mühimmat yoksa missiles boş listedir.

**Örnek:** tick 42 tamamlandığında publisher iki ayrı mesaj yollar: blue/tick 42 ve red/tick 42. İkisinin session_id, tick ve sim_time_s alanları aynı olur. Ağda aynı anda ulaşmaları gerekmez; ortak adım bu alanlarla anlaşılır.

`packet_no` uçak başına takip edilir: iki uçakta aynı numara bulunabilir. Sıra/kayıp kontrolünün anahtarı `(session_id, aircraft_id)` olur. `tick` ve packet_no aynı olmak zorunda değildir; örneğin her ikinci fizik dış adımında yayın yapılabilir. Paket numarası kaybolan veriyi geri getirmez.

Ayrı mesajlar ayrı UDP portu zorunluluğu yaratmaz. Hepsi aynı porta gönderilip aircraft_id ile ilgili kod bloğuna yönlendirilebilir. Port ayarları bu şablonlarda henüz belirlenmedi. Bir kontrolcü rakibinin durumunu istiyorsa rakibin ayrı mesajını da okuyabilir; mesajları ayırmak radar veya erişim kısıtı eklemez.

## Uçak alanları

| Alan | Tip | Anlam / birim |
|---|---|---|
| `id`, `name`, `team` | string | Ajan ID, görünen isim ve takım |
| `lat_deg`, `lon_deg` | number | Jeodezik enlem/boylam, derece |
| `alt_msl_m` | number | Deniz seviyesine göre irtifa, metre |
| `yaw_deg`, `pitch_deg`, `roll_deg` | number | Yönelim, derece |
| `tas_mps` | number | Gerçek hava hızı, m/s |
| `velocity_ned_mps` | array[number] | Kuzey, doğu, aşağı hız bileşenleri; m/s; aşağı pozitif |
| `mach` | number | Boyutsuz Mach |
| `health` | number | BVRGym healthPoints |

Yaw burun yönüdür, yer üzerindeki hareket yönü olmak zorunda değildir. Ayrıca heading kopyası gönderilmez. Publisher yalnızca gereken birim dönüşümlerini ve protokol biçimlendirmesini yapar; mesafe, açı, manevra veya taktik hesabı yapmaz. Recorder ham veri yaklaşımını korur. Yayınlanan veri gerçek simülasyon durumudur; radar ölçümü değildir.

## Füze alanları

| Alan | Tip | Anlam |
|---|---|---|
| `id` | string | Taşıyıcı + mühimmat anahtarı: blue/missile/0 |
| `owner_id` | string | Mesajın aircraft_id değeriyle aynı taşıyıcı ID |
| `target_id` | string veya null | Atanmış hedef ID; radar/arayıcı kilidi değildir |
| `state` | string | ready, in_flight, ended |
| `reserved` | bool | Atış planı için ayrılmış mı? Uçuş durumundan bağımsız |
| `outcome` | string veya null | hit, lost, other; sonuç yoksa null |
| `flight_time_s` | number veya null | Atış sonrası füzenin kendi simülasyon zamanı; hazırsa null |
| `kinematics` | object veya null | Uçakla aynı kinematik alanlar; id/name/team/health içermez |

Ready mühimmatın kinematics ve flight_time_s alanları null olur. Atış sonrasında kinematics dolar. Ended füzede son kinematik durum korunur; uçuş devam ediyor gibi yorumlanmaz. Bitmiş füzeler envanter listesinde aynı ID ile kalabilir.

Rezervasyon henüz uygulanmış bir BVRGym alanı değildir. Uygulanana kadar reserved false yayınlanır; rezerveli örnek gelecekteki kullanım içindir. Lost, BVRGym target_lost durumunu ifade eder; sensör kilidi kaybı değildir. State/outcome sınıflandırması protokol katmanında yapılacak, recorder'a hesap eklenmeyecek.

## Tek uçak komut mesajı

| Alan | Tip | Anlam |
|---|---|---|
| `schema_version` | int | 2 |
| `type` | string | aircraft_command |
| `session_id` | string | Komutun ait olduğu çalıştırma |
| `aircraft_id` | string | Komut verilen tek uçak |
| `command_id` | int | Bu uçak için oturum içinde komut kimliği |
| `based_on_tick` | int | Kararın üretildiği ortak durum adımı |
| `action` | object | Tek uçak için heading_deg, altitude_m, throttle |
| `events` | array | Tek seferlik olaylar; olay yoksa boş liste |

Action yön/irtifa/gaz değerlerini içerir; irtifa MSL metre, yön derece, gaz modelin izin verdiği normalize aralıktadır. `apply_action([heading, altitude, throttle])` çağrısına karşılık gelir.

Fire olayının alanları: `event_id`, `type: fire`, `target_id`. Atıcı üstteki aircraft_id'dir; ayrıca shooter_id gönderilmez. Hedef başka uçak olabilir fakat mesaj başka uçağın uçuş komutunu içeremez.

Komut kimliği `(session_id, aircraft_id, command_id)` ile; tek seferlik olay `(session_id, aircraft_id, event_id)` ile takip edilir. Aynı olay tekrar gelirse ikinci atış üretmemelidir. Python hedefi bağlar ve atışın kabul/red sonucunu bildirir; bu cevap şeması henüz kesinleşmedi. Belirli füze seçimi için isteğe bağlı missile_id ve rezervasyon komutları, gerekli destek eklenmeden kullanılmaz.

## Ayrı kontrol kodları, ortak simülasyon adımı

Önerilen çağrı sırası:

1. Tick 42 durumunu blue ve red kontrol blokları ayrı ayrı değerlendirir.
2. Blue kontrol bloğu yalnızca blue komutunu oluşturur; red bloğu yalnızca red komutunu oluşturur.
3. Python tarafı aynı based_on_tick = 42 için iki komutu toplar.
4. İki komut tamamlanınca **tek bir ortak step()** çağrısı bütün ajanları ilerletir ve CSV kaydını alır.
5. Tick 43 için ayrı durum mesajları yayınlanır.

Bir uçak komutu geldi diye bütün dünyayı hemen ilerletmeyiz; aksi halde diğer kontrolcü eski durum üzerinden komut verebilir. İlk sürüm önerisi, bütün kontrollü ajanların aynı adım için geçerli komutu gelmeden beklemektir. Eksik komutta kendiliğinden manevra üretme veya önceki komutla ilerleme davranışı eklenmez.

Kontrol için onaylanan yaklaşım hâlâ C++ içine Python gömülmesi ve doğrudan fonksiyon çağrılarıdır. Komut JSON'ları veri sözleşmesini gösterir; ağdan JSON göndermek veya her adımda dosya okumak gerekmez. Ayrı C++ kod blokları ayrı işletim sistemi süreçleri anlamına gelmez; ayrı süreçler istenirse bağlantı tasarımı ayrıca ele alınır.

## Kapsam

Yalnızca mesaj şablonları ve açıklamaları güncellendi. Publisher, C++ bağlaması, rezervasyon mekanizması veya simülasyon davranışı uygulanmadı.
