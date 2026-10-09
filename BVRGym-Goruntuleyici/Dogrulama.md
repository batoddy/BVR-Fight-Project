# Tacview CSV altyapısı doğrulaması

8 Ekim 2026 güncellemesi. Görüntüleyicinin tek simülasyon veri kaynağı BVRGym'in yerleşik Tacview CSV kayıtlarıdır.

- Python tarafında 15 kontrol geçti: yerleşik sütunlar, uçak/füze zamanları, boş mühimmat dosyaları, isteğe bağlı ölçümler, hatalı satırlar, klasörün sonradan oluşması, dosyaların sabitlenmesi, yeni kayıtla değiştirilmesi, hata sonrası toparlanma, uzun kayıtlar ve salt okunur HTTP.
- JavaScript tarafında 15 kontrol geçti: konum/yön interpolasyonu, füzenin kayıt aralığı, ayrı uçaklar, sayfalı yükleme, yeni kayıt, tüm geçmişin korunması, altı oynatma hızı, duraklatma ve ileri/geri sarma.
- Gerçek BVRGym `TacviewLogger` ile iki F-16 ve üç aktif AIM-120 kaydedildi. Beş nesne ve 64 ölçüm doğrudan yerleşik CSV'lerden okundu. Dosyalardaki zaman, yön ve irtifa, simülasyon nesnelerinden okunan değerlerle birebir eşleşti.
- Kayıtlı ve kayıtsız aynı uçuşun nihai fizik sonuçları eşit bulundu. Kontrol edilen BVRGym logger, environment ve agent kaynaklarının içerik özetleri değişmedi.
- Tarayıcı kontrolünde gerçek CSV'lerle beş 3D model görüntülendi. Füze başlangıç zamanı, geriye sarma, aynı klasöre yeni kaydın yüklenmesi, 2× oynatma, 3D/2D, alt alanın boyutlandırılması ve dar ekran kontrolü geçti. JavaScript çalışma hatası görülmedi.
- BVRGym'in mevcut `data_output/test_BTvsBT` kaydı da değiştirilmeden açıldı. Mach/TAS sütunları bulunmadığından değerler uydurulmadı.

Tacview uygulamasında ayrıca açıp karşılaştırma yapılmadı. Uyumluluk, BVRGym'in kendi logger'ının çıktılarının değiştirilmeden kullanılması ve Tacview'in belgelenmiş CSV sözleşmesi üzerinden doğrulandı. İmha/isabet olayları yerleşik CSV'de bulunmaz; füze ölçüm aralığının bitmesi yalnızca kayıt sınırı olarak ele alınır.

UDP alıcısı, özel JSON yayın biçimi, port/nesne profilleri, görüntüleyiciye özel XML'ler, method observer, eski bağlantı testleri, başlatıcı betikler, eski ekran görüntüleri ve önceki arayüz yedekleri proje içinden kaldırılmıştır. BVRGym'in kendi FlightGear desteği ve kaynakları korunur.
