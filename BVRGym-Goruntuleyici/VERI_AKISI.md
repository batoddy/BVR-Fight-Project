# Görüntüleyici veri akışı — ajanlar için

Kaynak **Tacview sütun adları taşıyan CSV kayıtlarıdır**. İki üretici desteklenir: `Workspace/src/recorder.py` (bizim recorder'ımız; dosya adında ajan anahtarını taşır, yanına `manifest.json` ve `events.jsonl` yazar) ve BVRGym'in değiştirilmeyen yerleşik `TacviewLogger`'ı (ikisini de yazmaz). UDP, WebSocket, ACMI ve özel uçuş yayıncısı yoktur. Uçuş kodu görüntüleyiciyi import etmez; görüntüleyici simülasyona komut veya kayda yazı göndermez.

```text
BVRGym / JSBSim → recorder.py (veya yerleşik TacviewLogger) → nesne başına CSV
→ recordings.py → server.py / yerel HTTP JSON
→ SampleBuffer / ReplayClock → 2D veya 3D sahne
```

## Kayıt ve dizin akışı

`BVRGym/jsb_gym/utils/loggers.py` ölçümleri uçuşta bellekte toplar; `save_logs()` çıktı dizinini yeniden oluşturup CSV'leri yazar. `jsb_gym/envs/BaseEnv.py` içindeki `env.log_tacview()` bunu kullanır. Bu sistem tamamlanan kayıtları oynatır, canlı tick aktarmaz. Her simülasyon için ayrı alt klasör kullan.

Arayüzde **Kayıt aç → Kayıt klasörü değiştir** ile Windows’un yerleşik klasör penceresinden bir ana dizin seçilir. İptal mevcut dizini korur. Bu dizin ve doğrudan alt klasörlerindeki CSV kayıtları listelenir; uçuş listeden açılır. Dizin değişmesi açık uçuşu değiştirmez. Tarayıcı sadece dizin yolunu localStorage'da hatırlar; CSV yükleme ve IndexedDB son-kayıt deposu kaldırıldı. Açık uçuş klasörü sunucuda izlenir.

## CSV sözleşmesi

Dosya adı: bizim recorder'ımızda `F-16 (KE--blue) [Blue].csv` ve `AIM-120 AMRAAM (KE--blue--0) [Blue].csv`; yerleşik logger'da `F-16 (Blue) [Blue].csv` ve `AIM-120 AMRAAM (AIM0) [Red].csv`. Model, çağrı adı ve takım buradan okunur. Çağrı adındaki `--` ayrımı ajan anahtarını ve mühimmat numarasını verir; gösterilen ad ilk parçadır. Füze sınıflaması bilinen model ön eklerine dayanır; yalnız F-16/AIM-120 geometrileri vardır, diğer modeller genel simgeyle gösterilir. Bilinmeyen takım nötrdür.

| Sütun | Anlam / birim |
|---|---|
| Time | Ortak simülasyon saati, saniye |
| Longitude | Boylam, derece |
| Latitude | **Jeosantrik** enlem (`position/lat-gc-deg`), derece |
| Altitude | JSBSim radyal MSL irtifa, metre |
| Roll (deg), Pitch (deg), Yaw (deg) | Ölçülen yönelim, derece |
| Mach | Ölçülen Mach (`velocities/mach`) |
| TAS | İsteğe bağlı, m/s; yoksa `velocities/vt-fps` okunur |
| `position/lat-geod-deg` | Jeodezik enlem; görüntüleyici kullanmaz, ACMI ihracatı için saklanır |
| `velocities/v-*-fps` | NED hız bileşenleri, ft/s |
| `forces/load-factor`, `propulsion/total-fuel-lbs`, `fcs/throttle-cmd-norm` | Yük katsayısı, yakıt (lb), uygulanan gaz |
| `command/heading-deg`, `command/altitude-m`, `command/throttle-norm` | O snapshot'ta uygulanan dış uçuş komutu |

Zaman ve başlangıç konumu zorunludur; sütun sırası bağımsızdır. Seyrek boş konumlar son değeri kullanır. Eksik yönelim ölçüm gibi sunulmaz. Sayı/koordinat/zaman/negatif hız doğrulanır; aynı zamandaki son satır tutulur.

ECEF, jeosantrik enlem ve JSBSim radyal irtifasından hesaplanır; yerel düzlem için ayrıca jeodezik enlem bulunur. Ham CSV değerleri değişmez. Ölçülen hız `TAS` sütunundan, o yoksa `velocities/vt-fps`'ten ×0,3048 ile okunur — bu bir birim çevrimidir, türetme değildir. Ölçüm hiç yoksa son bir saniyedeki ECEF konum değişiminden `position_speed_mps` hesaplanır; bu TAS değildir. `estimated_mach` ISA tahminidir. Ölçülen TAS/Mach önceliklidir. Gösterim birimleri yalnız tarayıcıda değişir (varsayılan m/s, ft, NM).

Boş mühimmat CSV'si envantere alınır ama sahnede nesne oluşturmaz. Taşıyıcı sahipliği önce dosya adındaki ajan anahtarından çözülür (`ownership_source='filename'`); ad bunu taşımıyorsa takım kuralına düşülür (`'team'`) ve aynı takımda birden fazla uçak varsa sahiplik belirsiz kalır. Yerleşik logger aynı takımda aynı füze numarasını aynı dosyaya yazıp önceki veriyi ezebilir; arayüz yalnızca sahiplik gerçekten çözülemediğinde uyarır ve kaybolan ölçümleri üretemez.

`manifest.json` ve `events.jsonl` okunur; ikisi de klasör imzasına dahildir. Manifest koordinat/irtifa referansını doğrular ve farklı bir referans bildirilirse uyarı verir; özeti `packet.manifest` ile oturum paneline gider. Olaylar `packet.events` ile taşınır. Gösterim tek bir tercihe bağlıdır: `preferences.missileOutcomes` (Görünüm menüsündeki **Füze vuruş bilgisi**, varsayılan kapalı). Açıkken olaylar zaman çubuğuna işaretlenir ve `missile_hit` / `missile_lost` mühimmat durumunu **İsabet** / **Kayıp** yapar; kapalıyken işaret çizilmez ve mühimmat yalnızca ölçümün bittiğini söyleyen **Atıldı** durumunda kalır. Sonuç hiçbir durumda ölçümden çıkarılmaz. En fazla 5000 olay gösterilir, fazlası uyarıyla bildirilir.

## Kod ve API

- `viewer.py`: yerel sunucu, başlangıç klasörü ve port.
- `recordings.py`: CSV okuma, doğrulama, dizin kütüphanesi, bekleme ve atomik değişim.
- `telemetry.py`: konum hızı ve ISA Mach tahmini.
- `server.py`: `POST /api/choose-directory` yerleşik Windows klasör penceresini açar (`native_picker.py`); `POST /api/directory {path}` liste dizinini değiştirir; `GET /api/recordings` kayıtları listeler; `POST /api/open {id}` kayıt seçer. `/api/import` yoktur.
- `GET /api/state?after=…&session=…`: **schema_version=3**, source=tacview-csv. Ölçümler 2000 örneklik JSON parçalarıyla gelir; session değişimi eski tamponu sıfırlar, cursor tekrarları önler. catalog_root, recording, warnings ve inventory ayrı metadata'dır.
- Zaman çubuğu işaretleri ve oturum paneli `web/app.js` içindedir (`drawEventMarks`, `sessionRows`, `columnTable`); mühimmat durumu `web/units.js` içindeki `munitionState` ile belirlenir.
- `web/recording-menu.js`: arayüz dizin akışı. `web/stream.js`: tam örnek tamponu, interpolasyon, saat ve iz penceresi. `scene.js`/`scene2d.js`: çizim; 3D eksenleri x=doğu, y=MSL irtifa, z=eksi kuzey.

Klasör iki saniye değişmeyince okunur, işlem sonunda dosya imzası tekrar kontrol edilir. Hata halinde eski geçerli kayıt/adı korunur. CSV'de kesin tamamlanma işareti bulunmadığından çok uzun yazım duraklaması kesin ayırt edilemez; save_logs tamamlandıktan sonra açmak en güvenilir akıştır. Uçak ölçümü bitince son konum soluk/kesikli gösterilir; füzeler sadece kayıt aralıklarında görünür, isteğe bağlı eski izleri korunur.

Mimariyi değiştirirken uçuş kodundan arayüz bağımlılığı isteme, BVRGym kaynaklarına özel yayın ekleme ve CSV'leri yeniden yazma. Kullanım ve test komutları README.md'dedir.
