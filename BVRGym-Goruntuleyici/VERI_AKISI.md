# Görüntüleyici veri akışı — ajanlar için kısa not

Mevcut kaynak **BVRGym'in yerleşik `TacviewLogger` sınıfının ürettiği CSV dosyalarıdır**. Aynı dosyalar Tacview'de de açılır. UDP, WebSocket veya ACMI kullanılmaz. Uçuş kodu arayüzü import etmez; görüntüleyici simülasyona komut göndermez ve CSV'leri değiştirmez.

## Akış

```text
BVRGym / JSBSim → TacviewLogger → nesne başına CSV
→ Python görüntüleyici → yerel HTTP üzerinden JSON
→ tarayıcıdaki örnek tamponu → 2D / 3D sahne
```

BVRGym tarafında `jsb_gym/utils/loggers.py` kayıt sınıfını, `jsb_gym/envs/BaseEnv.py` ise `env.log_tacview()` bağlantısını içerir. Ölçümler uçuş sırasında **bellekte toplanır**; `save_logs()` çağrıldığında CSV'ler diske yazılır. Bu nedenle mevcut sistem **tamamlanan kayıtların oynatımıdır, canlı tick aktarımı değildir**. Uçuş erken kesiliyorsa oluşturulmuş logger'ın kaydı ayrıca kaydedilmelidir. Yerleşik `save_logs()` çıktı klasörünü yeniden oluşturur; farklı simülasyonlar için ayrı klasör kullan.

## CSV sözleşmesi

Her uçak veya füze ayrı dosyadır. Örnek adlar:

```text
F-16 (Blue) [Blue].csv
AIM-120 AMRAAM (AIM0) [Red].csv
```

Model, çağrı adı ve takım dosya adından okunur. Parser, modeli `AIM`, `AAM` veya `AMRAAM` ile başlayan nesneleri füze olarak tanır.

| Sütun | Anlam / birim |
| --- | --- |
| `Time` | Simülasyon zamanı, saniye |
| `Longitude`, `Latitude` | Boylam, enlem; derece |
| `Altitude` | Deniz seviyesine göre irtifa, metre |
| `Roll (deg)`, `Pitch (deg)`, `Yaw (deg)` | Yatış, yunuslama, heading; derece |
| `TAS`, `Mach` | İsteğe bağlı hız (m/s) ve Mach |

Zaman ve konum sütunları zorunludur; sütun sırası önemli değildir. Yerleşik logger TAS/Mach yazmadığında arayüz konum/zamandan yaklaşık hız, standart atmosferden yaklaşık Mach hesaplar. Boş mühimmat CSV'leri envanterde tutulur, sahnede uçan füze oluşturmaz. Füze kaydının bitmesi isabet/imha anlamına gelmez. Aynı takımda birden fazla uçak varsa kayıtta bulunmayan füze sahipliği tahmin edilmez.

## Görüntüleyici tarafı

- `viewer.py`: yerel sunucuyu başlatır; varsayılan adres `http://127.0.0.1:8765`.
- `recordings.py`: CSV okuma/doğrulama, dosya adından nesne bilgisi ve koordinat dönüşümü. Klasör modunda dosya değişimlerinin durulmasını bekleyip kaydı yeniden yükler.
- `telemetry.py`: eksik hız/Mach tahminleri.
- `server.py`: `GET /api/state?after=…&session=…` ile ölçümleri JSON parçaları halinde sunar. Paket sözleşmesi `schema_version=2`, `source=tacview-csv`; `cursor` ve `session` aktarım konumunu/kayıt değişimini belirler. `GET /api/recordings` klasörleri listeler; `POST /api/open` klasör seçer, `POST /api/import` tarayıcıdan seçilen CSV metinlerini alır.
- `web/app.js`, `web/stream.js`: paketleri alır, kaydı bellekte tutar; seçilen oynatma zamanında örnekler arasında interpolasyon yapar. Oynatma hızı simülasyonu hızlandırmaz, yalnızca gösterim saatini değiştirir.
- `web/scene.js`, `web/scene2d.js`: yerel doğu/kuzey ve metre cinsindeki irtifayla çizer. 3D eksenler `x=doğu`, `y=irtifa`, `z=-kuzey`. Kullanıcının birim seçimi yalnızca gösterimi değiştirir.

Arayüzden bilgisayardaki dosya/klasör seçimi **o anki içeriği içe aktarır**; sonraki değişiklikler için yeniden seçmek gerekir. Sunucunun katalog listesinden açılan veya `--recording` ile verilen klasör ise izlenir. Bu izleme, logger'ın bellekte tuttuğu henüz kaydedilmemiş ölçümleri okuyamaz. Son kayıtlar tarayıcıdaki IndexedDB'de hatırlanır.

**Mimari kural:** Yeni görseller ve hesaplamalar görüntüleyicide geliştirilmelidir. Uçuş koduna arayüz bağımlılığı veya BVRGym içine özel veri yayımlayıcı eklemeyin. Daha fazla kullanım bilgisi: [README.md](README.md).
