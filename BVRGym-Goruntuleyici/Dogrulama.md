# Görüntüleyici doğrulaması

9 Ekim 2026. BVRGym kaynakları ve özgün CSV dosyaları değiştirilmedi.

- **28 Python testi:** CSV doğrulama, sayfalama, koordinat dönüşümü, hız tahmini, envanter, HTTP, dizin seçimi ve hatalı kayıtta eski uçuş/adını koruma.
- **23 JavaScript testi:** zamanlama, interpolasyon, birimler, mühimmat durumları, iz penceresi ve biten uçak ölçümünün işaretlenmesi.
- **Ana tarayıcı kontrolü:** gerçek yerleşik logger ile 5 nesneli kayıt ve 1199,5 saniyelik 10 nesneli kayıt. Dizin değiştirme, kayıt seçme, yanlış kayıtta eski ölçümleri koruma, 2D/3D, birim dönüşümleri, kısayollar, takip, izler, tema, ölçek, açılır menüler, panel/dock boyutlandırma ve 430×850 ekran kontrol edildi; hata yok.
- **Ek tarayıcı kontrolü:** klasörlere tıklayarak gezinme, üst dizine çıkma, MiG/Meteor için genel geometri, nötr takım rengi, eksik heading için —/kapalı çizgi, biten uçak ölçümü için sabit/soluk gösterim ve çok uçaklı logger sınırı uyarısı; hata yok.
- **Koordinat referansı:** bilgisayardaki JSBSim 1.3.1 ile 59° jeosantrik enlem ve 5000 m radyal MSL irtifada yarıçap ve jeodezik enlem karşılaştırıldı. ECEF yarıçap farkı 0,001 m'den küçüktür. Kutup/ekvator ve radyal tırmanış ayrı test edilir.

Veri akışı VERI_AKISI.md'dedir. CSV'de yazım tamamlanma işareti bulunmadığından iki saniyelik sakinlik kesin tamamlanma garantisi değildir. Logger'ın aynı takım/füze numarası çakışması arayüzde uyarılır; kaybolan veri uydurulmaz. Tacview uygulamasında ayrıca görsel karşılaştırma yapılmadı.
