# Görüntüleyici veri akışı — ajanlar için

Kaynak **BVRGym'in değiştirilmeyen yerleşik TacviewLogger CSV dosyalarıdır**. Aynı dosyalar Tacview'de açılır. UDP, WebSocket, ACMI ve özel uçuş yayıncısı yoktur. Uçuş kodu görüntüleyiciyi import etmez; görüntüleyici simülasyona komut veya CSV'ye yazı göndermez.

```text
BVRGym / JSBSim → TacviewLogger → nesne başına CSV
→ recordings.py → server.py / yerel HTTP JSON
→ SampleBuffer / ReplayClock → 2D veya 3D sahne
```

## Kayıt ve dizin akışı

`BVRGym/jsb_gym/utils/loggers.py` ölçümleri uçuşta bellekte toplar; `save_logs()` çıktı dizinini yeniden oluşturup CSV'leri yazar. `jsb_gym/envs/BaseEnv.py` içindeki `env.log_tacview()` bunu kullanır. Bu sistem tamamlanan kayıtları oynatır, canlı tick aktarmaz. Her simülasyon için ayrı alt klasör kullan.

Arayüzde **Kayıt aç → Dizin değiştir → Bu dizini kullan** ile bir ana dizin seçilir. Bu dizin ve doğrudan alt klasörlerindeki CSV kayıtları listelenir; uçuş listeden açılır. Dizin değişmesi açık uçuşu değiştirmez. Tarayıcı sadece dizin yolunu localStorage'da hatırlar; CSV yükleme ve IndexedDB son-kayıt deposu kaldırıldı. Açık uçuş klasörü sunucuda izlenir.

## CSV sözleşmesi

Dosya adı: `F-16 (Blue) [Blue].csv` veya `AIM-120 AMRAAM (AIM0) [Red].csv`. Model, çağrı adı ve takım buradan okunur. Füze sınıflaması bilinen model ön eklerine dayanır; yalnız F-16/AIM-120 geometrileri vardır, diğer modeller genel simgeyle gösterilir. Bilinmeyen takım nötrdür.

| Sütun | Anlam / birim |
|---|---|
| Time | Ortak simülasyon saati, saniye |
| Longitude | Boylam, derece |
| Latitude | Yerleşik BVRGym için **jeosantrik** enlem, derece |
| Altitude | JSBSim radyal MSL irtifa, metre |
| Roll (deg), Pitch (deg), Yaw (deg) | Ölçülen yönelim, derece |
| TAS, Mach | İsteğe bağlı; TAS m/s |

Zaman ve başlangıç konumu zorunludur; sütun sırası bağımsızdır. Seyrek boş konumlar son değeri kullanır. Eksik yönelim ölçüm gibi sunulmaz. Sayı/koordinat/zaman/negatif hız doğrulanır; aynı zamandaki son satır tutulur.

ECEF, jeosantrik enlem ve JSBSim radyal irtifasından hesaplanır; yerel düzlem için ayrıca jeodezik enlem bulunur. Ham CSV değerleri değişmez. TAS yoksa son bir saniyedeki ECEF konum değişiminden `position_speed_mps` hesaplanır; bu TAS değildir. `estimated_mach` ISA tahminidir. Ölçülen TAS/Mach önceliklidir. Gösterim birimleri yalnız tarayıcıda değişir (varsayılan m/s, ft, NM).

Boş mühimmat CSV'si envantere alınır ama sahnede nesne oluşturmaz. CSV taşıyıcı kimliği içermez: aynı takımda tek uçak varsa eşleştirilir, birden fazlaysa sahiplik belirsizdir. Logger aynı takımda aynı füze numarasını aynı dosyaya yazıp önceki veriyi ezebilir; arayüz uyarır ama kaybolan ölçümleri üretemez. İsabet/imha olayı kaydedilmediği için uydurulmaz.

## Kod ve API

- `viewer.py`: yerel sunucu, başlangıç klasörü ve port.
- `recordings.py`: CSV okuma, doğrulama, dizin kütüphanesi, bekleme ve atomik değişim.
- `telemetry.py`: konum hızı ve ISA Mach tahmini.
- `server.py`: `GET /api/directories?path=…` dizin gezintisi; `POST /api/directory {path}` liste dizinini değiştirir; `GET /api/recordings` kayıtları listeler; `POST /api/open {id}` kayıt seçer. `/api/import` yoktur.
- `GET /api/state?after=…&session=…`: **schema_version=3**, source=tacview-csv. Ölçümler 2000 örneklik JSON parçalarıyla gelir; session değişimi eski tamponu sıfırlar, cursor tekrarları önler. catalog_root, recording, warnings ve inventory ayrı metadata'dır.
- `web/recording-menu.js`: arayüz dizin akışı. `web/stream.js`: tam örnek tamponu, interpolasyon, saat ve iz penceresi. `scene.js`/`scene2d.js`: çizim; 3D eksenleri x=doğu, y=MSL irtifa, z=eksi kuzey.

Klasör iki saniye değişmeyince okunur, işlem sonunda dosya imzası tekrar kontrol edilir. Hata halinde eski geçerli kayıt/adı korunur. CSV'de kesin tamamlanma işareti bulunmadığından çok uzun yazım duraklaması kesin ayırt edilemez; save_logs tamamlandıktan sonra açmak en güvenilir akıştır. Uçak ölçümü bitince son konum soluk/kesikli gösterilir; füzeler sadece kayıt aralıklarında görünür, isteğe bağlı eski izleri korunur.

Mimariyi değiştirirken uçuş kodundan arayüz bağımlılığı isteme, BVRGym kaynaklarına özel yayın ekleme ve CSV'leri yeniden yazma. Kullanım ve test komutları README.md'dedir.
