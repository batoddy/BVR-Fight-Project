# BVR uçuş kayıt görüntüleyicisi

Görüntüleyici, BVR Fight Project kaydedicisinin ve BVRGym TacviewLogger'ın CSV uçuş kayıtlarını ortak zaman üzerinde 2D/3D oynatır. Uçuş simülasyonundan bağımsızdır. Oynatma kayıt verisini okur; kayıt menüsündeki silme işlemi seçilen oturumun dosyalarını diskten kaldırır.

## Başlat

Proje kökünden:

```powershell
python .\BVRGym-Goruntuleyici\viewer.py --recording .\recordings
```

Tarayıcıda `http://127.0.0.1:8765` adresini aç. Görüntüleyici için Python standart kütüphanesi yeterlidir. Three.js modülleri lisanslarıyla birlikte yerel `web/vendor` klasöründe bulunur; normal oynatma CDN gerektirmez.

`--recording`, doğrudan bir oturum klasörü veya kütüphane dizini olabilir. Parametre verilmezse izlenen dizin `BVRGym/data_output/tacview` olur. Proje `recordings/` dizini varsa kayıt kütüphanesi olarak seçilir. Başka port için `--port 8766` kullanılabilir. Ctrl+C görüntüleyiciyi kapatır; ayrı çalışan simülasyona komut göndermez.

## Kayıt kütüphanesi

**Kayıt aç** menüsünde dizindeki kayıtlar ve bir alt seviyedeki oturum klasörleri listelenir. **Kayıt klasörü değiştir**, Windows'un klasör seçme penceresini açar. Dizin değişikliği açık uçuşu değiştirmez; listeden bir kayıt seçilince yükleme başlar. **Yenile** yeni oturumları listeler. Arama alanı kayıt adlarını filtreler ve listede en son değiştirilmiş kayıtlar üstte görünür.

Her kaydın yanındaki **×**, onay sonrasında o oturumdaki nesne CSV'lerini, `events.jsonl`, `manifest.json` ve `calculations.csv` dosyalarını siler. Başka dosyalar korunur; alt kayıt klasörü tamamen boş kalırsa kaldırılır. Bu, diskten silmedir ve geri alınamaz.

Seçilen dizinin yolu tarayıcıda hatırlanır; CSV içerikleri tarayıcı deposuna kopyalanmaz. Kayıt açıldığında dosyalar iki saniye sakinleştikten sonra doğrulanarak yüklenir. Kayıt tamamlandıktan sonra açmak en güvenilir kullanımdır; klasör izleme canlı telemetri protokolü değildir.

## Oynatma

| Kontrol | İşlev |
|---|---|
| Baştan / oynat-duraklat / sona | Temel oynatma |
| Zaman çubuğu | Kaydın herhangi bir anına git |
| Zaman kutusu | Saniye veya `dakika:saniye` gir |
| Boşluk | Oynat/duraklat |
| Sol / sağ ok | 1 saniye geri/ileri; Shift ile 10 saniye |
| Yukarı / aşağı ok | Oynatma hızını bir kademe değiştir |
| Hız seçimi | `0.25×`, `0.5×`, `1×`, `2×`, `3×`, `4×`, `5×`, `10×` |

Kısayollar metin alanı düzenlerken veya menü açıkken ilgili girişe öncelik verir. Hız değişikliği duraklatılmış kaydı kendiliğinden başlatmaz. Oynatma alanının üst kenarı sürüklenerek yüksekliği ayarlanabilir.

## Kamera, nesneler ve ölçek

3D/2D düğmeleri aynı zaman ve nesne verisini farklı görünümlerde çizer. Kamera menüsünde serbest kamera, üstten ve tüm nesneler seçenekleri vardır. Sol tuş döndürür, sağ tuş taşır, tekerlek yakınlaştırır. Seçili nesne takibi görünüm menüsünden açılır; uçağa veya etikete çift tıklamak yaklaşır ve takip eder.

Sol panelde uçaklar takım ve adla filtrelenir. Göz düğmesi uçağı ve bağlı mühimmatını gizler. Envanterde uçaktaki, havadaki ve kaydı biten mühimmat ayrı görünür; uçuş ölçümü olmayan slot sahnede hayali bir füze oluşturmaz.

**Nesne ölçeği** `1×`, `25×`, `50×`, `100×` seçeneklerine sahiptir. `1×` gerçek model boyunu kullanır; diğer ölçeklerde füzeler görünürlük için ek `3.75×` büyütülür. Dünya mesafeleri ve hesaplanan menziller değişmez. Tanınmayan model genel geometriyle gösterilir.

## Analiz ve bilgi paneli

Seçili uçağın ölçülen irtifa, yönelim, TAS/Mach, yük faktörü ve gaz komutu sağ panelde görünür. Hava hızı ölçümü yoksa konumdan türetilen hız, Mach ölçümü yoksa ISA tahmini ayrı adlandırılır. Gaz, ölçülen motor çıkışı değil kayıttaki hedef komuttur.

C++ kullanılan oturumda `calculations.csv` varsa panel aşama, ATA, bearing, yatay/3B mesafe, aspect, WEZ ve karşılıklı WEZ durumlarını gösterir. MAR/DOR mevcut modelde `0` döner. Son snapshot gibi hesabın bulunmadığı anlarda arayüz bu eksikliği belirtir.

**Sahnede göster** bölümünden hedef çizgisi, ATA/bearing/range/aspect etiketi, radar açısı ve iki uçağın WEZ halkaları seçilir. Füze seçiliyse analiz taşıyıcısının, seçim yoksa uygun dost uçağın bakışından yürür. Bu çizimler C++ hesabını gösterir; yeni bir radar veya menzil çözümü yapmaz.

## Görünüm tercihleri

- İz süresi 1 dakika, 5 dakika veya tüm kayıt olarak seçilir.
- Kaydı biten füzeler son konumlarında tutulabilir. Bu, hâlâ uçtukları veya isabet ettikleri anlamına gelmez.
- İrtifa dolgusu, yükseklik çizgileri, hız vektörü okları, yer ızgarası ve etiket alanları ayrı ayarlanır.

**Görünüm → Hız vektörleri · oklar**, uçak ve füzelerin burun yönü yerine gerçek hareket yönünü gösterir. Kayıttaki kuzey/doğu/aşağı hızları varsa önceliklidir; yoksa ardışık konumların zamana göre farkı kullanılır. 3D ok tırmanma/dalış bileşenini de içerir; 2D'de yatay izdüşümü çizilir. Ok uzunluğu hızla doğrusal orantılıdır: 2D'de 100 m/s için 18 piksel, 3D'de aynı oran kamera hedefinin derinliğinde kullanılır. Aynı görünümde bütün nesneler ortak hız ölçeği kullanır; model büyütmesi ok uzunluğunu değiştirmez. Hız yönü bilinmiyorsa, nesne duruyorsa veya ölçümü bitmişse ok gösterilmez.
- **Füze vuruş bilgisi** varsayılan kapalıdır. Açıkken olay dosyasındaki sonuçlar envanterde ve zaman çizelgesinde gösterilir; olay işaretine basmak o ana gider.
- Tema, panel ölçüleri ve görünüm tercihleri tarayıcıda saklanır. Yan paneller kapanabilir ve genişlikleri değiştirilebilir.

## Birimler ve coğrafi referans

Yeni kayıtlar irtifayı ft, hızı kt, C++ hesap mesafelerini NM olarak saklar.
Görüntüleyici bu dosyaları, eski metre/ft-s kayıtlarını ve BVRGym native
CSV'lerini birlikte destekler. Okuma sırasında yapılan birim dönüşümü kaynak
dosyaları değiştirmez.

Varsayılan gösterim hız için m/s, irtifa için ft, mesafe için NM'dir. Ayarlarda hız `m/s`, `km/s`, `km/sa`, `kt`, `Mach`; irtifa `ft/m`; mesafe `NM/km` olarak seçilir. Birim seçimi kaynak dosyaları veya dünya koordinatlarını değiştirmez.

İrtifa referansı manifestten okunur. WGS84 elipsoid yüksekliği MSL gibi etiketlenmez; jeoit dönüşümü uygulanmaz. Zemin 0 m çizim referansıdır, arazi modeli değildir. Ayrıntılı koordinat ve kayıt sözleşmesi [veri referansındadır](../docs/veri_info/SIMULASYON_VERI_REFERANSI.md).

## Harita

**Görünüm → Harita · OpenStreetMap**, 2D ve 3D için isteğe bağlı zemin görsellerini açar. Varsayılan kapalıdır ve yer ızgarasından bağımsızdır. Görseller tarayıcı tarafından `tile.openstreetmap.org` adresinden yüklenir. Katman kapalıyken yeni istek başlatılmaz; hata uçuş oynatımını durdurmaz.

Harita uçaklarla aynı yerel koordinat referansına yerleştirilir. Arazi yükseltisi içermez. Web Mercator sınırı dışındaki kutup bölgelerinde harita gösterilmez. Kaynak bilgisi açık katmanın yanında görünür.

## Eksik veya hatalı kayıt

Hatalı CSV/olay verisi hata olarak gösterilir ve önceki geçerli kayıt korunur. Eksik yönelim bilinmiyor olarak belirtilir. Uçağın ölçümleri bittiğinde son konum soluk gösterilir. Füze uçuşu yalnız kayıt aralığında oynatılır; sonuç yalnız olaydan öğrenilir. Aynı takımda birden fazla olası taşıyıcı varsa sahiplik tahmin edilmez.

Kodun veri akışı [VERI_AKISI.md](VERI_AKISI.md), testler ve doğrulama kapsamı [Dogrulama.md](Dogrulama.md) içinde açıklanır.
