# Görüntüleyici doğrulaması

9 Ekim 2026. Veri kaynağı BVRGym'in değiştirilmeyen yerleşik Tacview CSV kayıtlarıdır.

- Python: 23 test; CSV doğrulama, tüm geçmişin saklanması, atomik kayıt değiştirme, hatalı dosyada önceki kaydı koruma, dosya seçimi, klasör seçimine geri dönme ve HTTP erişimleri.
- JavaScript: 18 test; konum ve açı interpolasyonu, kayıt sınırları, sayfalama, altı oynatma hızı, duraklatma ve ileri/geri sarma.
- Tarayıcı: yerleşik logger ile üretilen iki F-16 ve üç füze, ayrıca değiştirilmeden okunan 10 nesneli test_BTvsBT kaydı. CSV dosyalarını arayüzden açma, hatalı seçimi reddetme, klasör değiştirme, son kayıtları yeniden açma, 2× oynatma, füze izlerini koruma, panel genişliği, daraltılan oynatma alanı, tercihlerin yenilemede korunması ve 430×850 dar ekran yerleşimi kontrol edildi. Çalışma hatası görülmedi.
- Önceki altyapı kontrolünde gerçek TacviewLogger ölçümleri fizik nesneleriyle karşılaştırıldı; kayıtlı/kayıtsız uçuş sonuçları aynı bulundu. Bu arayüz güncellemesi BVRGym kaynaklarını veya uçuş kodlarını değiştirmez.

Yerleşik logger CSV dosyalarını save_logs çağrısında üretir: bu altyapı kayıt oynatır, canlı veri sağlamaz. CSV'de TAS/Mach yoksa hız konum değişiminden, Mach standart atmosferden yaklaşık olarak hesaplanır; bilgi panelinde hesaplandığı açıklanır. Kompakt etiketlerde üstte isim ve hız, altta ortalanmış irtifa gösterilir. Hız tam sayıya yuvarlanır, ayrıntı panelinde ondalık değer korunur. İsabet/ölüm olayı üretilmez. Tacview uygulamasında ayrıca görsel karşılaştırma yapılmadı.

Eski UDP, özel yayın, port profili, XML bağlantısı ve başlatıcı sistemleri yoktur. Kayıt seçimi yalnızca görüntüleyicinin açık kaydını değiştirir; uçuş komutu API'si bulunmaz.


Son görsel güncellemede gerçek kayıtlarla envanterin uçak altında gruplanması, boş mühimmat dosyalarının envantere alınması, zamanla Uçakta/Havada/Atıldı durumları, tahmini hızın yaklaşık 300 m/s okunması, ft/NM birimleri, uçak ve füzenin ortak büyütmesi, gerçek model uzunlukları, irtifa çizgisinin doğru uç noktaları, heading çizgisinin ölçülen yönle eşleşmesi ve yer ızgarası kapalıyken irtifa göstergesinin çalışması kontrol edildi.

İrtifa dolgusu güncellemesinde çizgilerin önceki ince opaklığı korundu. Uçuş izinden yere inen mavi perdenin alfa değeri yukarıda 1, yerde 0 olacak şekilde değişir; malzeme opaklığıyla birlikte yumuşak bir dolgu verir. Dolgunun yalnızca uçaklarda oluşturulduğu, yer referansına doğru uzandığı ve İrtifa düğmesiyle kapandığı tarayıcıda doğrulandı. Burun yönü çizgisi için -30°, 0°, +30° pitch ve 0°, 90°, 180°, 270° heading kombinasyonlarında 3D yön ve model burnundaki başlangıç konumu kontrol edildi.
