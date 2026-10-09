# BVRGym uçuş kayıt görüntüleyicisi

Veri kaynağı BVRGym'in yerleşik `TacviewLogger` sınıfının ürettiği CSV dosyalarıdır. Aynı dosyalar Tacview ve bu arayüzde açılır. BVRGym kaynak kodları değiştirilmez; uçuş kodu görüntüleyiciyi import etmez. Görüntüleyici kayıtları yalnızca okur, dosyalara veya simülasyona yazmaz.

## Başlatma ve kayıt açma

Workspace klasöründe, kendi Python ortamın etkin durumdayken:

```powershell
python .\BVRGym-Goruntuleyici\viewer.py
```

Tarayıcıda http://127.0.0.1:8765 adresini aç. Arayüzde **Kayıt aç** ile bir simülasyonun CSV dosyalarını birlikte veya CSV klasörünü seç. Kayıt değiştirmek için sunucuyu yeniden başlatmak gerekmez.

Kayıt penceresinde Workspace/recordings ve BVRGym/data_output altındaki kayıt klasörleri de listelenir. Bu listeden açılan klasör izlenir; yeni kayıt tamamlanınca kendiliğinden yüklenir. Bilgisayardan seçilen CSV dosyaları o anki içerikleriyle açılır; dosyalar sonradan değişirse yeniden seçmelisin. Ham dosyalar değiştirilmez.

Son beş açılan kayıt tarayıcıda hatırlanır. Bilgisayardan seçilen kayıtların CSV içerikleri de bu tarayıcının yerel depolamasında tutulur. Tarayıcı verileri temizlenirse son kayıt listesi sıfırlanır. Kayıtların Tacview uyumluluğu değişmez.

İstersen başlangıçta izlenecek klasörü terminalden de verebilirsin. Görüntüleyici klasöründe:

```powershell
python .\viewer.py --recording "..\recordings\agent_ucus"
```

Varsayılan klasör Workspace/BVRGym/data_output/tacview'dir. BVRGym'in kendi klasöründe `python -m tests.test_tacview` bu kaydı oluşturur. Farklı port için `--port 8766` kullan. Ctrl+C yalnızca görüntüleyiciyi kapatır.

## Arayüz

- Başlık, kayıt ve kamera kontrolleri tek üst çubuktadır. Kayıt adına basarak süre ve tam kaynak yolunu görebilirsin.
- Yan paneller iç kenardaki oklarla kapanır; kenarları sürüklenerek genişlikleri ayarlanır.
- Oynatma alanının üst kenarını sürükleyerek yüksekliğini değiştir. Temel kontroller daraltıldığında görünür kalır. Hızlar 1×, 2×, 3×, 4×, 5×, 10×; hız değişikliği duraklatmayı bozmaz.
- Zaman alanına saniye veya dakika:saniye yazarak git. Boşluk oynatır/duraklatır, sağ/sol ok 1 saniye, Shift ile 10 saniye sarar.
- Kamera menüsünde Serbest kamera, Üstten ve Tüm nesneler bulunur. Serbest kamera takibi bırakır; Takip düğmesi seçili nesneyi izler.
- Nesne listesinden arama/filtre yap, göz düğmesiyle nesneyi gizle. Füze kaydı bittiğinde seçim korunur; son ölçüm olduğu açıkça belirtilir.
- Görünüm menüsünde iz süresi, biten füze izleri, yer referansı ve etiketleri ayarla. Uçak/füze ölçeği dünya boyutuna göre sabittir; yakınlaştırdığında ekranda büyür.
- Tema, panel boyutları, görünüm ve etiket tercihleri tarayıcıda hatırlanır. Dar ekranda paneller sahnenin üzerinde açılır.

## Ölçek, birimler ve mühimmat

Hesaplama ve CSV verisi metre cinsinde kalır. Görüntüleme mesafeleri NM (1 NM = 1852 m), irtifa ft (1 ft = 0,3048 m), hız m/s, açılar derece kullanır.

F-16 görseli 15,06 m, AIM-120 görseli 3,66 m uzunluğa normalize edilir. Gerçek ölçek 1× seçildiğinde uzunluk oranı yaklaşık 4,1:1'dir. Diğer görünüm ölçeklerinde füzeler görünürlük için ayrıca %25 büyütülür; bu yalnızca görsel modeli etkiler. Varsayılan 50× okunabilirlik içindir: modelin boyu büyür, gerçek konumu/irtifası/mesafesi değişmez. Görünüm menüsünde 1× gerçek ölçek, 25×, 50× veya 100× seçebilirsin. Uzaklaştıkça model ekranda küçülür.

Tacview'in [sürüm geçmişi](https://www.tacview.net/download/latest/en/) uzak nesnelerde otomatik büyütme kullanımını belgeliyor; tam büyütme formülü yayınlanmış değil. Burada kullanıcının yakınlaştırma davranışı tercihine uygun sabit bir büyütme ve füzelere küçük bir görsel büyütme kullanılır. Fiziksel uzunluk kaynakları: [F-16](https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104505/f-16-fighting-falcon/), [AIM-120](https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104576/aim-120-amraam/).

İrtifa çizgileri yalnızca uçaklardan 0 m MSL düzlemine iner; füzelerde düşey çizgi veya yer halkası gösterilmez. Çizgiler önceki ince görünümünü korur. Uçuş izi ile yer düzlemi arasındaki alan, üstte mavi ve yere doğru tamamen şeffaflaşan bir perdeyle doldurulur. Dolgu iz süresiyle birlikte değişir. Arazi yüksekliği kullanılmadığı için bu AGL ölçümü değildir. Üst çubuktaki İrtifa düğmesiyle veya Görünüm menüsündeki İrtifa dolgusu ve çizgiler seçeneğiyle açılıp kapanır; tercih hatırlanır.

Kesikli yön çizgisi modelin doğrudan burnundan başlar ve üç boyutlu burun yönünü izler. Heading yanında pitch/yunuslama da çizgiye yansır; tırmanışta yukarı, dalışta aşağı yönelir. 2D'de üstten izdüşümü gösterilir. Hareket yönüyle aynı olmak zorunda değildir. Görünüm menüsünden bağımsız açılıp kapanır.

Hız ölçümü yoksa [Tacview'in yaklaşımına](https://raia-software-inc.gitbook.io/tacview/technical-documentation/formulas) benzer şekilde, son 1 saniyedeki üç boyutlu konum değişimi süreye bölünür. Başlangıçta mevcut kısa aralık kullanılır; ilk örnekte sonraki aralıktan tahmin yapılır. Tek örnekte hesaplanamaz. Rüzgâr kaydedilmediği için bu değer kesin hava hızı değildir; rüzgârsız uçuşta TAS'a yaklaşır. Mach, ISA sıcaklığından tahmin edilir. Kayıttaki TAS/Mach varsa daima önceliklidir. Bunlar CSV'ye yazılmaz.

Sol listede uçak kartları ve altlarında mühimmat bulunur. Boş yerleşik mühimmat dosyaları da sayılır. Kaydın ilk füze ölçümünden önce **Uçakta**, kayıt aralığında **Havada**, sonrasında koyu **Atıldı** görünür. Atıldı isabet/imha anlamına gelmez. Boş dosya yalnızca atış kaydı bulunmadığını gösterir.

BVRGym CSV dosya adında takım vardır, taşıyıcı uçak kimliği yoktur. Aynı renkte tek uçak varsa mühimmat ona bağlanır. Birden fazla uçak varsa sahip tahmin edilmez; ayrı, belirsiz mühimmat grubunda açıklanır. Kayıtta bulunmayan yük veya füze adedi uydurulmaz.

## Uçuş kodundaki kayıt

BVRGym environment kullanırken, kendi normal senaryo kodunda:

```python
baseEnv_conf.tacview_output_dir = "data_output/benim_senaryom"
# Environment oluşturulduktan ve reset edildikten sonra:
env.log_tacview()  # Yerleşik kayıt sınıfını oluşturur.
while not done:
    obs, reward, done, truncated, info = env.step(action)
    env.log_tacview()
```

Yerleşik kayıt çağrısı tamamlanan environment'ın verilerini diske yazar. Kendi döngünde erken çıkış için `finally` içinde `env.tacview_logger.save_logs()` kullanabilirsin; logger'ın oluşturulmuş olması gerekir. Her simülasyon için ayrı çıktı klasörü seç: BVRGym'in `save_logs()` metodu seçilen klasörü silip yeniden oluşturur.

`examples/tacview_flight.py`, BVRGym environment'ıyla kısa bir örnektir. `Workspace/src/agent_ucus.py` ise mevcut iki ajanlı uçuşu aynı yerleşik `TacviewLogger` ile kaydeder. Ham `F16BVR` nesneleriyle çalışan `Workspace/src/ucus.py`, aynı logger'a uçakları tanıtan küçük bir senaryo bağlamı kullanır. Bu kodların görüntüleyiciye bağımlılığı yoktur.

## Veri sözleşmesi

- Dosya adı: `MODEL (ÇAĞRI_ADI) [RENK].csv`. Uçak/füze modeli ve takım bu yerleşik Tacview adlandırmasından okunur; port tabanlı nesne ayarı yoktur.
- `Time`: simülasyon başlangıcından itibaren saniye. Füzelerin kaydında BVRGym taşıyıcı uçağın saatini kullanır.
- `Longitude`, `Latitude`: derece; `Altitude`: metre, deniz seviyesine göre.
- `Roll (deg)`, `Pitch (deg)`, `Yaw (deg)`: derece. Sütun sırası önemli değildir. Konum ve zaman zorunludur; yönelim yoksa sıfır kullanılır. İsteğe bağlı `TAS` (m/s) ve `Mach` sütunları varsa okunur. Yerleşik BVRGym logger'ı bunları yazmadığında konum ve zamandan yaklaşık hız, standart atmosferden yaklaşık Mach hesaplanır. Ölçülen ve hesaplanan değerler ayrı tutulur; hesaplanan değerler bilgi panelindeki başlık ve açıklamada belirtilir. Kompakt etiketlerin üst satırında solda ad, sağda tam sayıya yuvarlanmış hız; alt satırın ortasında irtifa bulunur; bilgi paneli ondalık hassasiyetini korur.
- Kullanılmayan mühimmatın boş CSV dosyalarından envanter bilgisi okunur; uçuş izi veya sahnede füze oluşturulmaz. Hatalı ölçüm veya geriye giden zaman, dosya ve satır bilgisiyle bildirilir; önceki geçerli kayıt korunur.
- Ham kayıt dosyaları değiştirilmez veya başka bir formata çevrilmez. HTTP'deki JSON yalnızca tarayıcıya sunulan iç gösterimdir; simülasyondan özel yayın gerektirmez.
- Tüm kayıt saklanır; eski 20.000 örnek sınırı yoktur. Dosyaların boyutuna göre bellek kullanımı artar.

## Kayıt zamanı ve oynatma

BVRGym'in yerleşik logger'ı ölçümleri uçuş sırasında bellekte toplar ve `save_logs()` çağrıldığında CSV'leri diske yazar. Bu yüzden bu altyapı kayıt oynatımı sağlar; simülasyon sürerken canlı veri aktarımı yapmaz. Arayüzün klasörü izlemesi bunu değiştirmez.

Oynatma hızı 1×, 2×, 3×, 4×, 5× ve 10× olarak ayarlanır. Zaman çubuğu tüm kaydı ileri/geri sarar. Uçaklar ilk kayıt zamanlarından önce görünmez; sonrasında son ölçümleri korunur. Füzeler yalnızca ölçüm bulunan zaman aralığında çizilir. Füze kaydının bitmesi isabet, hedef kaybı veya imha olayı olarak yorumlanmaz: yerleşik CSV bu olayları içermez.

3D/2D, kamera, nesne takibi, uçuş izi, yer referansı, yan paneller, boyutlandırılabilir alt kontrol alanı ve koyu mod korunmuştur. Arazi verisi kullanılmaz; yer düzlemi 0 m MSL referansıdır. Görüntüleyici bağımsızdır ve uçuş komutu API'si bulunmaz. Three.js yerel olarak, lisansıyla birlikte bulunur.

## Dosyalar ve doğrulama

- `viewer.py`: kayıt klasörünü ve yerel HTTP arayüzünü başlatır.
- `recordings.py`: yerleşik CSV'leri okur, doğrular ve klasör değişimlerini izler.
- `telemetry.py`: kayıttan hız ve standart atmosfer Mach tahminleri; özgün değerleri değiştirmez.
- `server.py`: kayıt seçimi ve tarayıcıya veri sunumu; uçuş komutu içermez.
- `web/`: görselleştirme ve ortak zamanlı kayıt oynatma.
- `tests/`: CSV, kayıt değişimi, HTTP, zamanlama ve gerçek BVRGym logger'ı için kontroller.

Görüntüleyici için Python standart kütüphanesi yeterlidir; BVRGym yalnızca simülasyonu çalıştıran Python ortamında kurulu olmalıdır. Testler: `python -m unittest discover -s tests -p "test_*.py"` ve `node --test tests/stream.test.mjs tests/units.test.mjs`. Gerçek simülasyon kontrolü BVRGym kurulu ortamda `python tests/integration_bvrgym.py --bvrgym "..\BVRGym" --output "..\recordings\verification"` ile yapılır.
