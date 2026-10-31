# Füze uçuş modeli: tek tablo + irtifa katsayısı

Füze atıldıktan sonra **ne zaman nerede, hangi hızda** olduğunu veren model. Kod: `calculations/missile_flight.hpp` (`glide_` fonksiyonları), veri: `model/missile_glide.hpp`.

R_cold formülü (`r_cold_formulu.md`) bunun yerine geçmez. R_cold yalnız "kaçan hedefe yetişir mi" sınırını verir, füzenin yol boyunca nerede olduğunu vermez.

## Model

```
1) Boost      tepe hız = atış hızı + Δv(h)
              Δv(h) = 889.3 + 19.48·x − 1.532·x²   m/s,   x = h − 10 km
              süre 0.37 s, sabit ivme: yol = (atış hızı + tepe hız) / 2 × 0.37

2) Süzülme    füze, 10 km'de ölçülmüş tek bir eğriyi izler; eğriye tepe hızının Mach'ından girer.
              aynı Mach aralığında:  yol  = k(h) × referans yol
                                     süre = k(h) × (a_10km / a_h) × referans süre
              k(h) = e^(0.11·x)

3) Bitiş      Mach 1'in altına inen füze kaybolur (BVRGym target_lost_below_mach = 1).
```

- **Atış hızı katsayı değil, kaydırmadır.** Hızlı atılan füze aynı eğriye daha yukarıdan girer, bu yüzden daha uzağa gider.
- **İrtifa katsayıdır.** Füzeyi yavaşlatan sürükleme hava yoğunluğuyla orantılıdır; yoğunluk irtifayla üstel düşer. R_cold'daki irtifa katsayısı da bu yüzden benzer çıkmıştı (`b = 0.1523`).
- **Süredeki `a_10km / a_h` çarpanı:** Eğri Mach ekseninde karşılaştırılıyor. Aynı Mach alçakta daha yüksek bir m/s demek; süre = yol / hız olduğu için ses hızı oranı süreye girer.

## Nasıl bulundu

Veri: `docs/fuze_analiz/sonuclar.txt`. Bu dosya 21 `mar_test` kaydından (4–10 km, atış 290–560 m/s) çıkarılmış hız–yol–süre tablolarını içerir; her irtifa için bir tablo vardır. Yol ve süre, kayıttaki hızın zamana göre integralidir.

1. **m/s ekseninde tek katsayı tutmadı.** Her irtifanın eğrisini 10 km eğrisine oranlayınca oran hız aralığına göre 0.18 ile 0.60 arasında değişti. Füze Mach 1.2–1.4 civarında (ses hızına yaklaşırken) farklı davranıyor; aynı m/s her irtifada farklı bir Mach demek.
2. **Mach ekseninde tuttu.** Aynı karşılaştırma Mach ekseninde yapılınca oran her irtifada neredeyse sabit çıktı:

   | İrtifa | Mach aralıklarındaki yol oranı (3.4→1.0) |
   |---|---|
   | 4 km | 0.41 – 0.58 |
   | 6 km | 0.58 – 0.70 |
   | 8 km | 0.76 – 0.88 |
   | 10 km | 1 |

3. **b seçimi.** k(h) = e^(b·x) için b denendi. Her irtifada 250, 300, 350 ve 400 m/s atışlar için tepe hızdan M2.5, M2, M1.5, M1.2 ve M1'e kadar olan yollar irtifanın kendi tablosuyla karşılaştırıldı:

   | b | Yol hatası RMS | En kötü |
   |---|---|---|
   | 0.100 | %3.3 | %7.2 |
   | 0.105 | %2.2 | %5.7 |
   | **0.110** | **%2.3** | **%8.5** |
   | 0.115 | %3.5 | %11.2 |

   0.11 seçildi, çünkü en önemli nokta olan Mach 1'e kadarki toplam yolda hata her irtifada %1.3'ün altında. En büyük hatalar alçakta, M2 civarındaki ara noktalarda (4 km'de −%8). Model orada füzeyi biraz geride gösteriyor.

4. **Boost.** 21 kaydın hepsinde boost yolu / ortalama hız 0.363–0.375 s çıktı, bu yüzden boost süresi 0.37 s alındı.

Karşılaştırma için: irtifa başına ayrı tablo kullanan eski kaydırma kontrolünde de hata %2–5'ti (`sonuclar.txt`, "Kaydırma kontrolü"). Tek tablo modeli bu kadar iyi.

## Sınırlar

- **İrtifa 4–10 km.** Dışındaki irtifa sınıra çekilir.
- **Yol düz kabul edilir.** Tablolar kaçan hedefe karşı, aynı irtifadaki atışlardan çıktı. Güdümün dönen hedefe karşı kaybettirdiği yol modelde yok. Füze uzak hedefe loft yapıyorsa (15 km'ye tırmanıp dalıyorsa) eğri tutmayabilir.
- **Füzeye özgü.** Model BVRGym AIM-120 ayarlarına göre kuruldu. BVRGym diğer tipleri de aynı fizikle uçurduğu için şimdilik bütün profiller `AIM120C_GLIDE` kullanıyor.
- **Hız fonksiyonlarını entegre etmeyin.** `glide_speed_at_...` o andaki hızı verir. Yol ya da süre gerekiyorsa `glide_distance_at_time_nm` ve `glide_time_at_distance_s` kullanılmalı; bunlar ölçülmüş yolu ve süreyi doğrudan verir.
