# Simülasyon mesaj şablonları — sürüm 1

Onaylanan mesaj tasarımının referans örnekleri. **Publisher veya C++–Python bağlantısı henüz uygulanmadı.** Sayılar temsili verilerdir; gerçek bir uçuş kaydı veya füze performans modeli değildir. Bu klasör simülasyonun çalışma ayarlarını değiştirmez.

| Dosya | Kullanım |
|---|---|
| [telemetry.json](telemetry.json) | Simülasyondan UDP üzerinden yayınlanacak durum paketinin örneği: iki uçak, dört hazır füze |
| [command.json](command.json) | C++ → Python komutlarının mantıksal yapısı; doğrudan fonksiyon çağrılarıyla da taşınabilir |
| [missile_state_examples.json](missile_state_examples.json) | Tek bir füzenin farklı durum örnekleri. Tam UDP paketi değildir; aynı kimliğin bu örnekleri aynı anda yayınlanmaz. |

## Yayın paketi

| Alan | Tip | Anlam |
|---|---|---|
| `schema_version` | int | Şema sürümü; ilk sürüm 1 |
| `session_id` | string | Her çalıştırma için yeni, benzersiz kimlik |
| `packet_no` | int | Oturum içinde artan sıra numarası; yeniden başlayan oturumda sıfırlanabilir |
| `sim_time_s` | number | Ortak senaryo zamanı, simülasyon saniyesi; duvar saati değildir |
| `aircraft` | array | Uçak durumları |
| `missiles` | array | Hazır mühimmat ve uçan/bitmiş füzelerin durumları |

Snapshot bütün ajanlar ilgili dış döngü adımını tamamladıktan sonra alınır. JSON biçimi önerilen yayın sözleşmesidir; native JSBSim UDP metin formatından farklıdır. Paket numarası, kaybolan paketin verisini geri getirmez; eksik/eski paketleri fark etmeye yarar.

## Uçak alanları ve birimler

| Alan | Tip | Anlam / kaynak |
|---|---|---|
| `id` | string | YAML ajan anahtarı; ör. `blue` |
| `name`, `team` | string | BVRGym configindeki ad ve takım |
| `lat_deg`, `lon_deg` | number | Jeodezik enlem ve boylam, derece |
| `alt_msl_m` | number | Deniz seviyesine göre irtifa, metre |
| `yaw_deg`, `pitch_deg`, `roll_deg` | number | JSBSim yönelim açıları, derece |
| `tas_mps` | number | Gerçek hava hızı, m/s |
| `velocity_ned_mps` | array[number] | Üç bileşen: kuzey, doğu, aşağı; m/s; aşağı pozitif |
| `mach` | number | Mach, boyutsuz |
| `health` | number | BVRGym `healthPoints` |

`yaw_deg` burun yönünü ifade eder; yer üzerindeki hareket yönüyle karıştırılmaz. Ayrıca heading kopyası yayınlanmaz. Publisher gerekli birim dönüşümlerini yapabilir; taktik, mesafe, göreli açı veya manevra hesabı yapmaz. Recorder ham veri yaklaşımını korur. Bunlar gerçek simülasyon durumlarıdır; radar/sensör ölçümü olarak yorumlanmaz.

## Füze alanları ve durumları

| Alan | Tip | Anlam |
|---|---|---|
| `id` | string | Taşıyıcı kimliği + mühimmat anahtarı; ör. `blue/missile/0` |
| `owner_id` | string | Taşıyıcı ajan ID |
| `target_id` | string veya null | Atanmış hedef ID; radar/arayıcı kilidi değildir |
| `state` | string | `ready`, `in_flight`, `ended` |
| `reserved` | bool | Atış planı için ayrılmış mı? Uçuş durumundan bağımsızdır |
| `outcome` | string veya null | Sonuç: `hit`, `lost`, `other`; sonuç yoksa null |
| `flight_time_s` | number veya null | Füzenin atış sonrasındaki kendi simülasyon zamanı; hazırsa null |
| `kinematics` | object veya null | Uçakla aynı kinematik alanlar; `id/name/team/health` içermez |

- `ready`: Atılmamış mühimmat. `flight_time_s` ve `kinematics` null; başlangıçtaki JSBSim konumu uçuş ölçümü gibi yayınlanmaz.
- `in_flight`: Atılmış ve uçuşu devam eden füze. Kinematik verileri doludur.
- `ended`: Uçuş sona ermiş. Son kinematik durum korunur; alıcı bunu hareketi devam eden nesne olarak yorumlamaz.
- `reserved: true`: Henüz tasarlanan kontrol katmanı özelliğidir. Mevcut BVRGym'de hazır rezervasyon alanı yoktur; uygulanana kadar false gönderilir.
- `lost`: BVRGym'in `target_lost` sonucudur; radar kilidi kaybını ifade etmez. `other` yeni bir fizik ölçümü değil, diğer bitiş durumları için protokol değeridir.

Füze state/outcome değerleri mevcut BVRGym alanlarından sınıflandırılacaktır. Bu sınıflandırma publisher/protokol katmanındadır; recorder içine hesap eklenmez. Füze ve ortak senaryo saatleri ayrı korunur.

## C++ → Python komutları

UDP durum yayınıdır. Kontrol için onaylanan yaklaşım, C++ içine Python gömülmesi ve doğrudan fonksiyon çağrılarıdır. `command.json` bu çağrıların veri sözleşmesini gösterir; JSON dosyasını ağdan göndermek veya her adımda diskten okumak zorunlu değildir.

| Alan | Tip | Anlam |
|---|---|---|
| `command_id` | int | Komut grubunun kimliği |
| `actions` | object | Ajan ID → uçuş komutu |
| `heading_deg` | number | Hedef yön, derece |
| `altitude_m` | number | Hedef MSL irtifa, metre |
| `throttle` | number | Gaz komutu; modelin izin verdiği aralık uygulanır |
| `events` | array | Tek seferlik işlemler; işlem yoksa boş liste |
| `event_id` | int | Tek seferlik olay kimliği |
| `type` | string | İlk sürümde `fire` |
| `shooter_id`, `target_id` | string | Atıcı ve hedef ajan ID'leri |

İlk sürümde her kontrol adımında bütün uçakların actions alanları gönderilir. Uçuş komutları `apply_action([heading, altitude, throttle])` ile uygulanır. Atış hedefi C++ tarafından seçilir; Python hedefi bağlar, atış isteğini uygular ve sonucu bildirir. Hedef seçimi, adım ilerletme ve rezervasyon fonksiyonları önceki tasarımda konuşuldu; bunlar için ayrı mesaj/cevap şemaları bu dosyalarda henüz kesinleştirilmedi.

`event_id` oturum içinde aynı atışı ikinci kez üretmemelidir. Oturum değişince kimlik takibi yeniden başlar. Atış kabul/red sonucu ve atılan füze ID'si doğrudan çağrı cevabında döndürülmelidir; cevap yapısı uygulama öncesinde netleştirilecek.

Belirli mühimmat için ileride isteğe bağlı `missile_id` eklenebilir. Mevcut `BaseBVRAgent.launch_missile()` hazır füzeyi kendisi seçtiğinden, bu seçim veya rezervasyon desteği uygulanmadan bu alan kullanılmaz.

## Değişiklik sınırı

Bu klasör yalnızca şablon ve açıklama içerir. Publisher, C++ bağlaması, rezervasyon mekanizması veya yeni simülasyon davranışı kurulmadı. Şema değişirse açıklamalar ve örnekler birlikte güncellenir; uyumsuz değişiklikte `schema_version` artırılır.
