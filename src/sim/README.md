# Python simülasyon katmanı

Bu paket BVRGym/JSBSim uçaklarını ve mühimmatını oluşturur, fizik durumunu C++'a gönderir, gelen komutları uygular ve oturumu kaydeder. Taktik kararın sahibi C++ otopilotlarıdır. Python'daki uçuş kontrolü bu kararların yön/irtifa/gaz hedeflerini fizik modeline uygular.

Kurulum ve terminal sırası [çalıştırma kılavuzunda](../../docs/CALISTIRMA_KILAVUZU.md), alanların tamamı [veri referansında](../../docs/veri_info/SIMULASYON_VERI_REFERANSI.md) açıklanır.

## Dosyalar

| Dosya | Görev |
|---|---|
| [sim_main.py](sim_main.py) | Senaryo, TCP, kayıt ve dış adım döngüsünü birleştirir |
| [scenario.py](scenario.py) | Dosya yolları, YAML okuma ve temel senaryo doğrulaması |
| [scenario.yaml](scenario.yaml) | Süre, adım tekrarları, bağlantı ve ajan başlangıçları |
| [bvrgym_setup.py](bvrgym_setup.py) | Ajan başına config kopyaları, BVRGym nesneleri ve kontrol kurulumu |
| [simulation.py](simulation.py) | Ölçüm okuma, uçuş komutu doğrulama, açık atış ve fizik adımı |
| [flight_control.py](flight_control.py) | Yatış ve yük faktörüyle yön/irtifa kontrolü |
| [connector.py](connector.py) | Tek TCP istemcisi ve satır başına JSON mesajı |
| [recorder.py](recorder.py) | ft/kt ölçüm CSV'leri, NM/kt hesapları, olay ve manifest yazımı |
| [_sim_local_template.py](_sim_local_template.py) | TCP/C++ olmadan sabit veya elle değiştirilen komutlarla uçuş |

## Kurulum ve konfigürasyon

`load_scenario()` bu paket içindeki `scenario.yaml` dosyasını okur. Yollar terminalin konumundan değil kaynak dosyasının konumundan türetilir. `validate_scenario()` pozitif tamsayı `sim_time_step`, sonlu/negatif olmayan süre ve bekleme ile boş olmayan ajan listesi ister. Komut alanlarının doğrulaması fizik ilerlemeden önce ayrıca yapılır.

`create_agents()` BVRGym'i yoluna ekler ve nesne oluştururken geçici olarak model dizinine geçer. Her ajan için F-16 ve füze ayarları derin kopyalanır; bir uçağın ayarı diğerine sızmaz. Nesne kurulumu başarısız olsa da çağıranın çalışma dizini geri yüklenir.

`type`, C++ profil etiketidir. Uçak fiziği `F16BVR`, mühimmat fiziği `AAMBVR` olarak kurulur. `ammo` kadar slot oluşturulur. Native XML çıkışı ve BVRGym içindeki gerçek zaman beklemesi kapatılır; beklemeyi `sim_main.py` yönetir. Füze config'inde `dive_at=25000` kullanılır.

## Bir adım

1. Başlangıç ölçümü, ilk komutlarla kaydedilir.
2. `read_state()` ölçümleri ve son uygulanan uçuş komutlarını üretir; fizik ilerlemez.
3. Python `state` yollar ve aynı `tick` için `command` bekler.
4. C++ hesapları henüz ilerlememiş ölçümün zamanıyla yazılır.
5. `launch` listesindeki açık atış emirleri uygulanır.
6. Bütün uçuş komutları doğrulanır; ajanların `apply_action()` çağrıları yapılır.
7. `commands` güncellenir, `tick` artırılır ve yeni ölçüm kaydedilir.
8. Varsa `sleep_s` kadar gerçek zaman beklenir.

Süre kontrolü ilk uçağın fizik saatindeki başlangıca göre yapılır. Bağlantı bekleme süresi fizik süresine eklenmez. Son ölçüm C++'a gönderilmeyebilir; bu nedenle ölçüm sayısı ile hesap adımı sayısı aynı olmak zorunda değildir.

## Uçuş komutları ve atış

`Simulation.step()` her uçak için tam bir `{id, heading_deg, altitude_m, throttle}` komutu ister. Bilinmeyen/tekrarlanan kimlik, eksik uçak, sonlu olmayan sayı, yönün `0..360` veya gazın `0..1` dışına çıkması hatadır. Komut listesi tamamen doğrulanmadan hiçbir uçak ilerletilmez. Ana döngüdeki atış emirleri bu doğrulamadan önce işlendiği için tüm mesajın işlemsel geri alınması sağlanmaz.

`fire(shooter_id, target_id)`, kendine atışı ve hazır mühimmat yokluğunu `False` ile sonuçlandırır. Hazır ilk slotun hedefini bağlar ve fırlatır. Havadaki başka füze yeni atışı engellemez. `launch()` sonrası yeniden başlatma, taşıyıcının gerçek hava hızını m/s olarak kullanır; bu, BVRGym fırlatma yolundaki Mach/m/s uyuşmazlığını bu katmanda karşılar.

## Fizik ve kontrol zamanları

Mevcut modelde fizik `dt` yaklaşık `1/120 s`, `Control_time_step` değeri `10`'dur. Dış adım yaklaşık `dt × Control_time_step × sim_time_step` sürer. YAML'da `sim_time_step=1` yaklaşık 12 dış adım/s verir. Bu alan saniye değildir.

Uçak `F16BVR._step()` içinde yön ve irtifa hatasını bir iç çevrim başında hesaplar; kontrol yüzeyleri her fizik alt adımında güncellenir. `flight_control.py` kendi integral hesabında `DT_S=1/120` kullanır; fizik adımı değiştirilirse bu varsayım da gözden geçirilmelidir.

## Yön ve irtifa kontrolü

`install()` her uçağın kendi `AircraftPIDAutopilot` nesnesine kontrol fonksiyonunu bağlar. Dikey ivme isteği irtifa hatası ve dikey hız sönümünden gelir. Büyük yön hatasında sert dönüş yatışı, küçük hatada koordineli dönüş yatışı kullanılır. Elevator PI kontrolü, yatışta irtifayı koruyacak yük faktörünü izler.

| Ayar | Mevcut değer / rol |
|---|---|
| `K_ALT`, `K_VS` | `0.04`, `0.4`; irtifa hatası ve dikey hız |
| `A_MAX` | `3 m/s²`; dikey düzeltme sınırı |
| `N_MIN`, `N_MAX` | `0.5`, `6 g`; kontrolün istediği yük sınırları |
| `TAU_HEAD` | `2 s`; küçük yön hatasını kapatma süresi |
| `BIG_ALT_CHANGE_M` | `1500 m`; üzerinde BVRGym'in özgün kontrolü kullanılır |

Büyük irtifa hatasına geçildiğinde integral sıfırlanır. Normal bölgede integral mevcut elevator komutundan başlatılır. Bu kontrol, C++ profilindeki `max_g` veya `max_mach` değerlerinden otomatik ayarlanmaz.

## Kayıt

Yeni kayıt şeması `3`'tür: irtifa `alt_ft`, gerçek hava hızı `tas_kt`, NED
hızları `vel_north_kt/vel_east_kt/vel_down_kt`, irtifa komutu `cmd_alt_ft` olarak
yazılır. C++ hesap mesafeleri `_nm`, kapanma hızları `_kt` alanlarındadır.
JSBSim'in feet ve knot propertyleri doğrudan okunur; NED hızları ft/s'den kt'ye,
Python irtifa komutu metreden feet'e çevrilir. Kaynak ve çarpan manifestte bulunur.
Python uçuş mesajları ve YAML başlangıç ayarları metre/m/s kullanmaya devam eder.
Görüntüleyici yeni ve eski kayıtların birimlerini okuma sınırında çözer.

`Recorder.record()` bir dış snapshot yazar. Okunabilen propertyler ilk kullanımda belirlenir; modelde olmayan ek alanın sütunu açılmaz. Uçaklar her snapshot'ta, füzeler uçuşa başladıktan sonra ve bitiş durumları bir kez yazılarak kaydedilir. Hazır slotlar başlıklarıyla envanterde kalır.

Olaylar durum değişimlerinden oluşur: hedef ataması/kaldırılması, fırlatma, isabet, kayıp ve sağlık değişimi. Olay zamanı değişimin görüldüğü snapshot zamanıdır. `record_calculations()` C++ verisini tekrar hesaplamadan yazar ve aynı uçağın son ölçüm zamanını ekler. Alan kümesi ilk hesap mesajından sonra sabit olmalıdır.

`manifest.json`, senaryoyu, koordinat referansını, kaynak propertyleri, gerçek fizik adımını ve dosya/satır sayılarını açıklar. Başlangıçta ve kapanışta yazılır; her snapshot'ta yenilenmez. Alan ve birim ayrıntıları veri referansındadır.

## Yerel şablon

`_sim_local_template.py`, `Simulation` ve `Recorder` kullanır; ağ açmaz. Döngüde `commands` listesi değiştirilerek uçuş hedefi verilebilir. C++ çağrılmadığından otopilot hesap dosyası oluşmaz. `src/temp/` içindeki eski deneme dosyaları normal giriş yolu değildir.
