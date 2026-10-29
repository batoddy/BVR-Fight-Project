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

`type`, C++ profil etiketidir. Uçak fiziği `F16BVR`, mühimmat fiziği `AAMBVR` olarak kurulur. `ammo` kadar slot oluşturulur. Native XML çıkışı ve BVRGym içindeki gerçek zaman beklemesi kapatılır; beklemeyi `sim_main.py` yönetir. Füze config'i, uçakla aynı `Sim_time_step` dışında BVRGym'deki haliyle kullanılır.

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

`Simulation.step()` her uçak için tam bir `{id, heading_deg, altitude_m, throttle}` komutu ister; isteğe bağlı `g_limit` (`1.5..9 g`) dönüş yükünü sınırlar ve `cmd_g_limit` sütununa yazılır. Bilinmeyen/tekrarlanan kimlik, eksik uçak, sonlu olmayan sayı, yönün `0..360` veya gazın `0..1` dışına çıkması hatadır. Komut listesi tamamen doğrulanmadan hiçbir uçak ilerletilmez. Ana döngüdeki atış emirleri bu doğrulamadan önce işlendiği için tüm mesajın işlemsel geri alınması sağlanmaz.

`fire(shooter_id, target_id)`, kendine atışı ve hazır mühimmat yokluğunu `False` ile sonuçlandırır. Hazır ilk slotun hedefini bağlar ve fırlatır. Havadaki başka füze yeni atışı engellemez. Fırlatma BVRGym `launch()` ile olduğu gibi yapılır. BVRGym `reset()`'e taşıyıcının Mach sayısını m/s diye verdiği için füze neredeyse sıfır hızla başlar; bu uyuşmazlık bu katmanda düzeltilmez.

## Fizik ve kontrol zamanları

Mevcut modelde fizik `dt` yaklaşık `1/120 s`, `Control_time_step` değeri `10`'dur. Dış adım yaklaşık `dt × Control_time_step × sim_time_step` sürer. YAML'da `sim_time_step=1` yaklaşık 12 dış adım/s verir. Bu alan saniye değildir.

Uçak `F16BVR._step()` içinde yön ve irtifa hatasını bir iç çevrim başında hesaplar; kontrol yüzeyleri her fizik alt adımında güncellenir. `flight_control.py` kendi integral hesabında `DT_S=1/120` kullanır; fizik adımı değiştirilirse bu varsayım da gözden geçirilmelidir.

## Yön ve irtifa kontrolü

`install()` her uçağın kendi `AircraftPIDAutopilot` nesnesine kontrol fonksiyonunu bağlar; BVRGym kaynağı değişmez. Zincir: irtifa hatası → istenen dikey hız → istenen dikey ivme → yatış → hedef g → elevator PI. Büyük irtifa değişimi de aynı yoldan, dikey hız sınırıyla yapılır; BVRGym'in kendi kontrolüne devredilmez.

Dönüşte çekilecek en büyük yük komuttaki isteğe bağlı `g_limit` alanından gelir (yoksa `DEFAULT_TURN_G`). Büyük yön hatasında sert dönüş: yatış `acos((1 + a/g) / g_limit)`, yatış oturmadan çekilir; alçalma istenirse yatış 90°'ye yaklaşır ve uçak burun aşağı döner (out). Küçük hatada koordineli dönüş, yatış `acos(1 / g_limit)` ile sınırlı (crank, beam, recommit için düşük `g_limit`). Elevator, yatışta istenen dikey ivmeyi verecek yükü izler. Elevator 1 s'den uzun tam çekişte kalırsa (F-16 uçuş kontrol bilgisayarı yavaş hızda yükü sınırlar) yatış ulaşılan g'ye göre kurulur.

| Ayar | Mevcut değer / rol |
|---|---|
| `K_H`, `K_VZ` | `0.08`, `0.5` 1/s; irtifa hatası → dikey hız → dikey ivme (ω 0,2 rad/s, aşmasız) |
| `VZ_UP_MAX`, `VZ_DOWN_MAX` | `60`, `120 m/s`; tırmanma/alçalma hız sınırı |
| `A_UP_MAX`, `A_DOWN_MAX` | `1 g`; dikey ivme komutu sınırı |
| `A_UP_HARD` | `0.3 g`; sert dönüşte yatışı küçülten yukarı düzeltme sınırı (öncelik dönüş) |
| `DEFAULT_TURN_G` | `6 g`; komutta `g_limit` yoksa |
| `N_MIN` | `0.5 g`; alt yük sınırı |
| `ENTRY_G` | `1 g`; sert dönüşe girişte yatış oturmadan izin verilen ek dikey yük |
| `SATURATED_S` | `1 s`; tam çekiş süresi, sonrası yatış ulaşılan g'ye göre |
| `TAU_HEAD` | `2 s`; küçük yön hatasını kapatma süresi |

Gaz komutu doğrudan uygulanır. F-16 modelinde `0..0.5` rölantiden askeri güce (MIL), `0.5..1.0` art yakıcı aralığıdır (`fcs/throttle-cmd-norm` × 2, motor `augmethod 2`).

Ölçüm (180° dönüş, 6 g): 5 km / 460 m/s'de 169°'ye 24 s, en fazla +235 ft; 3300 ft alçalma komutuyla 25 s ve hız 905 → 957 kt. 3 g crank 50°: 14.5 s, +224 ft.

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
