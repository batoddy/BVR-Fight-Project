# BVRGym uçuş kayıt görüntüleyicisi

Veri kaynağı BVRGym'in yerleşik `TacviewLogger` sınıfının ürettiği CSV dosyalarıdır. Aynı dosyalar Tacview ve bu arayüzde açılır. BVRGym kaynak kodları değiştirilmez; uçuş kodu görüntüleyiciyi import etmez. Görüntüleyici kayıtları yalnızca okur, dosyalara veya simülasyona yazmaz.

## Başlatma

Görüntüleyici klasöründe:

```powershell
python .\viewer.py --recording "..\Workspace\recordings\agent_ucus"
```

Tarayıcıda http://127.0.0.1:8765 adresini aç. Farklı bir tarayıcı portu için `--port 8766` kullan.

BVRGym'in kendi örneğiyle kullanmak için BVRGym klasöründe:

```powershell
python -m tests.test_tacview
```

Görüntüleyici klasöründe:

```powershell
python .\viewer.py
```

Klasör belirtilmediğinde kardeş `Workspace/BVRGym/data_output/tacview` klasörü okunur. Başka bir yerleşik senaryo için `--recording "..\Workspace\BVRGym\data_output\test_BTvsBT"` kullanılabilir. Yollar çalışma klasörüne göre yorumlanır; BVRGym'in varsayılan yolu görüntüleyici dosyasının konumundan hesaplanır.

Arayüzü simülasyondan önce veya sonra açabilirsin. Kayıt klasörü henüz yoksa bekler. CSV'ler kaydedilip dosyalar sabitlenince klasördeki tüm kayıtlar birlikte yüklenir. Aynı klasöre yeni simülasyon kaydedildiğinde arayüz yeni kayda geçer. Ctrl+C yalnızca görüntüleyiciyi kapatır.

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
- `Roll (deg)`, `Pitch (deg)`, `Yaw (deg)`: derece. Sütun sırası önemli değildir. Konum ve zaman zorunludur; yönelim yoksa sıfır kullanılır. İsteğe bağlı `TAS` (m/s) ve `Mach` sütunları varsa okunur. Yerleşik BVRGym logger'ı bunları yazmadığından o kayıtlarda bu göstergeler boş kalır.
- Kullanılmayan mühimmatın boş CSV dosyaları atlanır. Hatalı ölçüm veya geriye giden zaman, dosya ve satır bilgisiyle bildirilir; önceki geçerli kayıt korunur.
- Ham kayıt dosyaları değiştirilmez veya başka bir formata çevrilmez. HTTP'deki JSON yalnızca tarayıcıya sunulan iç gösterimdir; simülasyondan özel yayın gerektirmez.
- Tüm kayıt saklanır; eski 20.000 örnek sınırı yoktur. Dosyaların boyutuna göre bellek kullanımı artar.

## Kayıt zamanı ve oynatma

BVRGym'in yerleşik logger'ı ölçümleri uçuş sırasında bellekte toplar ve `save_logs()` çağrıldığında CSV'leri diske yazar. Bu yüzden bu altyapı kayıt oynatımı sağlar; simülasyon sürerken canlı veri aktarımı yapmaz. Arayüzün klasörü izlemesi bunu değiştirmez.

Oynatma hızı 1×, 2×, 3×, 4×, 5× ve 10× olarak ayarlanır. Zaman çubuğu tüm kaydı ileri/geri sarar. Uçaklar ilk kayıt zamanlarından önce görünmez; sonrasında son ölçümleri korunur. Füzeler yalnızca ölçüm bulunan zaman aralığında çizilir. Füze kaydının bitmesi isabet, hedef kaybı veya imha olayı olarak yorumlanmaz: yerleşik CSV bu olayları içermez.

3D/2D, kamera, nesne takibi, uçuş izi, yer referansı, yan paneller, boyutlandırılabilir alt kontrol alanı ve koyu mod korunmuştur. Arazi verisi kullanılmaz; yer düzlemi 0 m MSL referansıdır. Görüntüleyici bağımsızdır ve uçuş komutu API'si bulunmaz. Three.js yerel olarak, lisansıyla birlikte bulunur.

## Dosyalar ve doğrulama

- `viewer.py`: kayıt klasörünü ve yerel HTTP arayüzünü başlatır.
- `recordings.py`: yerleşik CSV'leri okur, doğrular ve klasör değişimlerini izler.
- `server.py`: salt okunur dosya ve kayıt API'si.
- `web/`: görselleştirme ve ortak zamanlı kayıt oynatma.
- `tests/`: CSV, kayıt değişimi, HTTP, zamanlama ve gerçek BVRGym logger'ı için kontroller.

Görüntüleyici için Python standart kütüphanesi yeterlidir; BVRGym yalnızca simülasyonu çalıştıran Python ortamında kurulu olmalıdır. Testler: `python -m unittest discover -s tests -p "test_*.py"` ve `node --test tests/stream.test.mjs`. Gerçek simülasyon kontrolü BVRGym kurulu ortamda `python tests/integration_bvrgym.py --bvrgym "..\Workspace\BVRGym" --output "..\Workspace\recordings\verification"` ile yapılır.
