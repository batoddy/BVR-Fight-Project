# Otopilot kod standartları

Bu belge `src/otopilot` için geçerlidir. Yeni kod bu kurallara göre yazılır; bir kurala uymayan eski kod gördüğünde aynı değişiklikte düzelt.

## 1. Katmanlar ve kim neyi yazar

| Klasör | Görev | Yazdığı veri |
|---|---|---|
| `io/` | JSON ↔ C++ çevirisi | `Aircraft`'ın ölçüm alanları, `command` (Python'un son uyguladığı) |
| `model/` | Veri tipleri, `World::update` | `World::update`: `enemy_id`, `picture`, `weapons`'ın hesaplanan alanları, füzelerin `target` bilgisi |
| `calculations/` | Saf hesap | Hiçbir şey yazmaz; sonuç döndürür |
| `control/` | Karar ve manevra | `command`, `phase`, atış emri (`weapons.fire`) |
| `common/` | Birim, açı, vektör, koordinat yardımcıları | — |

- **Hesap fonksiyonları saftır.** Girdiyi yalnız parametrelerden alır. `Aircraft` alabilir, ama yalnız ölçülen (`kinematics`, `weapons.missiles`) ve sabit (`profile`) alanlarını okur, **`picture`'ı okumaz**. Başka bir hesabın sonucu gerekiyorsa onu parametre olarak alır (ör. `nez_nm(r_cold_nm, out)`).
- **Hesap sırası yalnız `World::update`'te durur.** Hangi hesabın hangisinden önce geldiği orada okunur.
- **Karar ve manevra kodu hesap yapmaz, `picture`'ı ve `weapons`'ı okur.** Yeni bir değer gerekiyorsa `calculations/`'a fonksiyon, `Picture`'a (kaçış) ya da `Weapons`'a (benim füzem) alan eklenir.
- **Silah hesapları ana silahtan yapılır.** WEZ, NEZ, F-pole, R_cold ve MAR `profile.main_weapon` ile hesaplanır; füze slotlarından (`weapons.missiles`) profil çekilmez.
- Her alanın yorumunda onu kimin yazdığı belirtilir.

## 2. Roller

| Kelime | Anlamı |
|---|---|
| `own` | bakan uçak; hesap onun gözünden yapılır |
| `enemy` | `own`'un atanmış düşmanı (`own.enemy_id`) |
| `other` | genel geometride karşı nesne (uçak ya da füze) |
| `missile`, `target` | bir füze ve **onun** hedefi |
| `threat` | bana doğru gelen düşman füzesi |
| `fighter`, `hostile` | rollerin somut adları; **yalnız** `World`, `Sim` ve `io/` içinde |

- Düşman ataması `World`'dedir. Hesap ve karar kodu "kim düşman" sorusunu bilmez, `(own, enemy)` parametre çiftiyle çalışır.
- Parametre sırası: `(own, enemy)`, `(own, other)`, `(missile, target)`, ardından sayılar.

## 3. Tip seçimi

- **Durum tutmayan fonksiyon grubu → `namespace`** (`geometry::`, `engagement::`, `turn::`). Çağrılırken namespace adı her zaman yazılır; `using namespace` kullanılmaz. Dosyaya özel yardımcılar `.cpp` içinde isimsiz namespace'te durur.
- **Durum tutan ya da bir kuralı koruyan → `class`**: özel üyeler `_` ile biter (`state_`).
- **Yalnız veri → `struct`**: alanlar `public`, sonlarında `_` yoktur. Struct'ta yalnız basit `const` sorgu fonksiyonları olur (`in_flight()`).
- **Durumlar `enum class`'tır**, `int` değildir.
- **Değeri düzgün tutulması gereken veri setter'la yazılır.** Örneğin `FlightCommand::set_heading_deg` yönü ±180'e sarar, `set_throttle` 0..1'e sıkıştırır. Setter'lar zincirlenebilir: `command.set_throttle(1.0).set_g_limit(6.0)`.

## 4. "Değer yok"

- Olmayabilen **sayı** → `std::optional<double>`. Kullanım: `if (const auto ttg = ...) { *ttg }`, `value_or(...)`, `has_value()`.
- Olmayabilen **nesne** → `const T*` (`nullptr` = yok). Zincirli `a()->b` yazılmaz; önce kontrol edilir.
- Sentinel değer (`-1`, `0` "yok" anlamında) kullanılmaz. Tel (JSON/CSV) biçimi gerektiriyorsa dönüşüm yalnız `io/` içinde yapılır.
- Vector'e indeksle erişmeden önce boyutu kontrol edilir.

## 5. İsimlendirme

- **Kod İngilizce, yorumlar Türkçe.** Sınıf adı olarak `Otopilot` kalır.
- **Biçim:** fonksiyon ve değişkenler `snake_case`, tipler `PascalCase`, sabitler `UPPER_SNAKE_CASE`, enum değerleri `PascalCase`.
- **Fiziksel değerin adı birimle biter:** `_ft`, `_m`, `_nm`, `_kt`, `_mps`, `_s`, `_deg`, `_rad`, `_g`. Birimsiz oranlar ve Mach eksiz kalır.
- **`get_` öneki yok:** `range_3d_nm()`, `last_fired()`.
- **Bool'lar soru gibi okunur:** `is_`, `has_`, `in_` önekleri (`is_pitbull`, `has_launch_order`, `enemy_in_wez`).
- **Tek harfli isim** yalnız kısa döngü sayacında ve yorumda yazılı bir formülün sembolünde kullanılır (ör. R_cold'daki `x`, `y`; dönüşteki `n`).
- **Aynı kavram her yerde aynı adı taşır:**
  - `closure_kt`: aradaki mesafenin azalma hızı (iki uçağın hareketi)
  - `enemy_closure_kt`: yalnız düşmanın bana doğru hız bileşeni
  - `r_cold_nm`: kaçarken düşman füzesinin yetişebileceği en büyük mesafe
- **İzinli kısaltmalar:** `ata`, `los`, `ned`, `wez`, `mar`, `nez`, `dor`, `tas`, `ttg`, `nm`, `kt`, `ft`. Bunların dışında kısaltma kullanılmaz.

## 6. Birimler

- **Saklanan değerler:** irtifa ft, mesafe NM, hız kt, süre s, açı derece.
- **Açılar içeride her zaman ±180.** 0..360 yalnız Python sınırında (`io/`) kullanılır.
- **SI'ya çevirme:** formül gerektiriyorsa çeviri fonksiyonun içinde yapılır; yerel değişkenin adı birimi taşır (`speed_mps`, `turn_rad`).
- **Sabitler** `common/units.hpp`'dedir. Kodun içinde sihirli sayı yazılmaz; ayar ya profile, ya manevra/otopilot sınıfının birimli bir sabitine konur. Doğrudan yazılabilen sayılar: 0, 1, 2, 90, 180, 360.

## 7. Manevra ve karar

- **Manevra yaşam döngüsü:** `enter(own)` girişte bir kez çağrılır, `update(own)` her step `own.command`'dan başlayıp komutu döner, `status(own)` bitti mi sorusuna cevap verir.
- **Kendi içinde biten manevralar** (Out gibi) hedefe ulaşınca `Done` döner. **Sürekli manevralar** (Crank, Beam, Hold, In) hep `Running` döner; onları otopilot bitirir. Bu manevralar karara yardım eden sorgular sunabilir (ör. `Crank::in_position`).
- **Geçiş koşulları** `control/conditions.hpp` içinde isimli fonksiyonlar olarak durur. Karar kodu bu koşulları çağırır; böylece karar yapısı değişse de (switch → karar ağacı) koşullar aynı kalır.
- **Otopilot** (`OtopilotFighter`, `OtopilotHostile`) `step` içinde tek switch'tir; her durum bir blok: `fly(own, manevra)` ile uç, sonra çıkış koşullarına bak. Durum yalnız `go_to(durum, manevra, own)` ile değiştirilir (`state_ = ...` elle yazılmaz); böylece yeni manevranın `enter()`'ı atlanmaz.

## 8. Dosya düzeni

- Her `.hpp`/`.cpp` çifti tek bir konuyu içerir ve dosyanın adı bu konuyu söyler.
- **Include sırası:**
  1. Dosyanın kendi başlığı
  2. Boş satır
  3. Standart kütüphane başlıkları (alfabetik)
  4. Boş satır
  5. Proje başlıkları (alfabetik)
- Kullanılan her şeyin başlığı doğrudan eklenir; dolaylı include'a güvenilmez.

## 9. Formüller ve yorumlar

Hesap ve manevra kodu bir insanın formülü kâğıttan takip eder gibi okuyabileceği şekilde yazılır:

- **Formül önce yorumda, sonra kodda.** Fonksiyonun üstünde formül açık yazılır (`NEZ = R_cold + yaklaşma + dönerken kaçamadığımız yol`); kod aynı sırayla, aynı adlarla ilerler.
- **Ara sonuçlar isimli değişkenlerdir.** Uzun tek satırlık ifade yerine anlamı olan adımlar: `flight_time_s`, `enemy_approach_nm`, `worst_case_nm`.
- **Kapalı kütüphane kısayolları kullanılmaz:** `std::hypot` yerine açık Pisagor (`std::sqrt(kuzey * kuzey + doğu * doğu)`), `std::clamp` ve iç içe `std::min`/`std::max` yerine `if`. Tek bir `std::max(a, b)` formülde "büyük olanı" diye geçiyorsa kalabilir. `atan2`, `sin`, `cos`, `sqrt`, `exp` gibi matematik fonksiyonları normaldir.
- **Seçim `if` ile yazılır**, iç içe ya da uzun `?:` ile değil. Kısa bir durum geçişi (`return in_mar ? Out : Hold;`) kalabilir.
- **Hız × süre hesapları** `travel_distance_nm(speed_kt, time_s)` ve `travel_time_s(distance_nm, speed_kt)` ile yazılır; formülde `3600` görünmez.

- **`.hpp`:** fonksiyonun ne döndürdüğü, birimi, ne zaman "yok" döndüğü.
- **`.cpp`:** formül ve türetmesi.
- **Alan yorumu:** `birim, anlam | kaynak ya da yazan`.
- **Ölü kod:** yorum satırına alınmış kod bırakılmaz; eski hali git'te durur. Planlanan iş `// TODO:` satırıyla belirtilir.
- **Bilinen ama bilerek dokunulmayan bir mantık hatası** `// NOT:` ile açıklanır.
- Kodu değiştiren, ilgili yorumu aynı değişiklikte günceller.

## 10. Biçim

- Allman parantez, 4 boşluk girinti, `if (` / `for (` / `while (` (anahtar kelimeden sonra boşluk).
- Değeri değişmeyen yerel değişken `const` olur.
- Derleme `-Wall -Wextra` ile **uyarısız** olmalıdır.

## 11. Derleme ve test

- `main.cpp` dışındaki bütün kaynaklar `otopilot_core` kütüphanesinde toplanır; `main` ve `units_test` bu kütüphaneye bağlanır.
- Her saf hesap fonksiyonunun `tests/units_test.cpp` içinde en az bir testi olur.
- Davranışı değiştirmesi beklenmeyen bir yeniden düzenlemeden sonra aynı senaryo koşturulur ve kayıtlar karşılaştırılır. Simülasyon deterministiktir; kayıtlar aynı çıkmalıdır.
