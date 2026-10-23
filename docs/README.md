# Proje belgeleri

Tarayıcıda okumak için [HTML teknik rehberi](PROJE_REHBERI.html) aç. [Tam otopilot UML](PROJE_REHBERI.html#otopilot-uml) bölümünde C++ sınıfları, struct ve enum tanımları, bütün üye değişkenleri, metotları ve aralarındaki ilişkiler bulunur. Sınıf seçerek diyagramı yakınlaştırabilir veya kutuya tıklayarak üye listesine gidebilirsin. Bütün bölümler tek uzun sayfada görünür; menü bağlantıları başlığa kaydırır. Arama bölümleri gizlemeden eşleşenleri işaretler. İnternet veya yerel sunucu gerekmez.

Belgeler çalışan kaynak kodunun sorumluluklarını anlatır. Kurulum, mimari, veri sözleşmesi ve geliştirme örnekleri ayrı yerlerde tutulur; aynı konunun ayrıntısı için aşağıdaki asıl kaynağa başvurulur.

| Belge | Cevapladığı soru |
|---|---|
| [Proje özeti](../README.md) | Proje ne yapar, hangi programlardan oluşur? |
| [Çalıştırma kılavuzu](CALISTIRMA_KILAVUZU.md) | Ortam nasıl kurulur, programlar nasıl başlatılır? |
| [Sistem mimarisi](mimari/BVR_SISTEM_MIMARISI.md) | Python, C++ ve görüntüleyici nasıl birlikte çalışır? |
| [Otopilot mimarisi](mimari/OTOPILOT_MIMARISI.md) | C++ sınıfları, veri sahipliği ve karar akışı nasıl düzenlenir? |
| [C++ kaynak rehberi](../src/otopilot/README.md) | Bir görev için hangi kaynak dosyasına gidilir? |
| [Otopilot kullanımı](../src/otopilot/KULLANIM.md) | Manevra, aşama, profil ve kayıt alanı nasıl eklenir? |
| [Python simülasyonu](../src/sim/README.md) | Senaryo, kurulum, fizik kontrolü ve kayıt nasıl yürür? |
| [Veri referansı](veri_info/SIMULASYON_VERI_REFERANSI.md) | Ölçüm, komut, olay ve CSV alanları ne ifade eder? |
| [TCP örnekleri](mimari/mesaj_sablonlari/tcp/README.md) | Geçerli mesajlar somut olarak nasıl görünür? |
| [Görüntüleyici kullanımı](../BVRGym-Goruntuleyici/README.md) | Kayıt açma, oynatma ve analiz araçları nasıl kullanılır? |
| [Görüntüleyici veri akışı](../BVRGym-Goruntuleyici/VERI_AKISI.md) | Dosyalar nasıl okunur ve sahneye taşınır? |
| [Doğrulama](../BVRGym-Goruntuleyici/Dogrulama.md) | Hangi testler vardır, nasıl çalıştırılır, ne doğrulanmıştır? |

## Referans verileri

`veri_info/` altındaki property katalogları, belirli bir JSBSim/model kurulumundan alınmış envanter çıktılarıdır. Çalışma zamanında kod tarafından okunmazlar. Katalog sürümü, birimler ve güncellik sınırı [veri referansında](veri_info/SIMULASYON_VERI_REFERANSI.md) açıklanır.

Markdown içindeki Mermaid blokları mimari ilişkileri özetler. HTML sürümünde özetler, tarayıcıda ek kütüphane gerektirmeyen SVG diyagramlarla gösterilir. Özet yerleşimleri `build_html.py` içindedir. Tam otopilot diyagramı ve üye listeleri ise `otopilot_uml.py` tarafından `src/otopilot` kaynaklarından üretilir; `.cpp` içindeki `Connection::Impl` de taranır. Üye sahipliği, kalıtım, imza/gövde içindeki tür kullanımları ve füzenin `target_id` ilişkisi ayrı çizgi türleriyle gösterilir. Standart kütüphane ve üçüncü taraf sınıfları açılmaz; yerel fonksiyon değişkenleri üye sayılmaz.

## HTML rehberi yeniden üretme

Proje kökünden, Python ortamı etkin halde:

```powershell
python -m pip install -r docs/requirements.txt
python docs/build_html.py
```

`PROJE_REHBERI.html`, mevcut C++, Markdown, CSV ve JSON kaynaklarından üretilir. HTML içeriğini elle değiştirmek yerine ilgili kaynağı düzenleyip yeniden üret. Yeni C++ türü eklenince `otopilot_uml.py` içindeki `LANES` yerleşimine adı da eklenmelidir; desteklenmeyen bir üye bildirimi veya yerleşimsiz bir tür sessizce atlanmaz, üretimi durdurur. Üretici tam UML yanında 12 ana belgeyi, alan kataloğunu ve TCP örneklerini de tek raporda toplar. Property kataloglarının tamamına rapor içindeki dosya bağlantılarından ulaşılır.

UML envanterini ve kaynak kapsamını denetlemek için:

```powershell
python -m unittest discover -s docs/tests
```

## Belgeleri geliştirirken

Bir davranışın sahibi hangi dosyaysa açıklama o bileşenin rehberine yazılır. Ortak veri alanları veri referansında tanımlanır. Çalışan davranış ile varsayım veya uygulanmamış hesap açıkça ayrılır. Test sonucu, çalıştırılmamış tarayıcı/uzun uçuş doğrulamasına genellenmez.
