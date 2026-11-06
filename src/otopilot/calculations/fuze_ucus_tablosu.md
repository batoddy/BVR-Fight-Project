# Füze uçuş modeli

Atılan füzenin hangi mesafede, hangi hızda ve ne zaman olacağı sabit ivmeli bir uyumla hesaplanır. Süre boost bitişinden sayılır. Parametreler `model/missile_profiles.hpp` içinde, füze profiline göre tutulur:

| Alan | Anlam | AIM-120C |
|---|---|---|
| `start_speed_kt` | Boost bitişindeki hız, kt | 2700 |
| `slowing_accel_kt_s` | Sabit yavaşlama, kt/s (negatif) | −18 |
| `pitbull_range_nm` | Arayıcının kendi kilitlendiği füze–hedef mesafesi, NM | 10 |
| `blast_radius_ft` | Patlama yarıçapı, ft | 10 |

Formüller (`common/formulas.hpp`):

```text
hız  v(t) = v0 + a·t                            kt
yol  x(t) = (v0·t + a·t²/2) / 3600              NM
süre t(x) = (−v0 + √(v0² + 2·a·x·3600)) / a     s   (kök içi negatifse füze o yola erişemez)
```

## Fonksiyonlar

| Fonksiyon | Dosya | Ne döndürür |
|---|---|---|
| `speed_at_time_kt` | `missile_geometry.cpp` | t saniyedeki hız |
| `distance_at_time_nm` | `missile_geometry.cpp` | t saniyede alınan yol |
| `time_at_distance_s` | `missile_geometry.cpp` | Yolu alma süresi; erişemezse yok |
| `catch_time_left_s` | `missile_geometry.cpp` | Havadaki füzenin hedefe yetişmesine kalan süre |
| `a_pole_nm`, `f_pole_nm`, `tr_nm` | `engagement.cpp` | Pole'lar ve en kısa atış menzili |

## Kullanılan yaklaşımlar

- **A-pole, F-pole:** füze uyumdaki gibi uçar, hedef yerinde kabul edilir. İki uçak arası mesafe şu anki `closure_kt` ile kapanır. 1 NM güvenlik payı düşülür.
- **TR:** atıştan pitbull'a kadar geçen sürede iki uçak `closure_kt` ile yaklaşır. TR, füze pitbull olduğunda mesafe DOR + 1 NM kalacak atış menzilidir. Denklemde kapanma füzenin başlangıç hızından düşülür: `t = time(DOR + pay − pitbull, v0 − kapanma, a)`.
- **Kalan yetişme süresi:** kapatılacak yol, füze–hedef mesafesinden patlama yarıçapı düşülerek bulunur. Başlangıç hızı olarak füze–hedef kapanma hızı (`target.closure_kt`) alınır, yavaşlama uyumdaki gibidir. Hedefin hızı sabit kabul edilir.

Uyumun ölçümleri `wez_grafikler/` ve `r_cold_grafikler/` klasörlerindedir. Çözüm yoksa fonksiyonlar `std::nullopt` döner.
