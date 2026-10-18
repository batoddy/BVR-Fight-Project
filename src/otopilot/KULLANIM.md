# Otopilotu Kullanmak: Örneklerle

Bu dosya "şunu yapmak istiyorum, hangi dosyaya ne yazarım?" sorusunu cevaplar.
Dosyaların tek tek ne olduğu [README.md](README.md)'de, derleme ve çalıştırma
[docs/CALISTIRMA_KILAVUZU.md](../../docs/CALISTIRMA_KILAVUZU.md)'dedir.

---

## İçindekiler

1. [Büyük resim: kod nerede çalışıyor](#1-büyük-resim-kod-nerede-çalışıyor)
2. [Verileri okumak](#2-verileri-okumak)
3. [Uçağı komutlamak](#3-uçağı-komutlamak)
4. [Geometri: mesafe ve yön](#4-geometri-mesafe-ve-yön)
5. [Füze atmak ve takip etmek](#5-füze-atmak-ve-takip-etmek)
6. [Manevralar](#6-manevralar)
7. [Hepsi bir arada: basit bir taktik](#7-hepsi-bir-arada-basit-bir-taktik)
8. [Değişken eklemek](#8-değişken-eklemek)
9. [Terminale bir şey yazdırmak](#9-terminale-bir-şey-yazdırmak)
10. [Sık yapılan hatalar](#10-sık-yapılan-hatalar)

---

## 1. Büyük resim: kod nerede çalışıyor

Python simülasyonu her adımda (saniyede yaklaşık 12 kez) C++'a uçakların durumunu gönderir
ve cevap gelene kadar bekler. C++ tarafında her adımda **tek bir fonksiyon** çalışır:
[autopilot.cpp](autopilot.cpp) içindeki `Autopilot::step`.

```
Python ── durum ──►  Autopilot::step  ── komut ──► Python
                     1. Ölçüm    JSON → world_           (hazır, dokunma)
                     2. Hesap    world_.update()         (mesafe, yön, WEZ ...)
                     3. Manevra  fighter_command_ = ...  (SEN: nasıl uçacak)
                     4. Atış     fighter_fire = ...      (SEN: füze atılsın mı)
                     5. Komut    CommandWriter::write    (hazır, dokunma)
```

Senin işin çoğunlukla **3. ve 4. adımlar**: elindeki veriye bakıp nasıl uçacağına ve ateş
edip etmeyeceğine karar vermek. 1 ve 5 hazır: JSON ile hiç uğraşmazsın, sadece C++ nesneleri
okur ve yazarsın.

**Nereye ne yazılır:**

| Yapmak istediğin | Dosya |
|---|---|
| Hangi manevra çalışsın, ateş edilsin mi | `autopilot.cpp` → `step` |
| Yeni manevra | `maneuver.hpp` + `maneuver.cpp` |
| Saf geometri (mesafe, yön, açı) | `geometry.hpp` + `geometry.cpp` |
| Füze ve taktik hesabı (WEZ, MAR, DOR) | `weapons.hpp` + `weapons.cpp` |
| Hesaplanan sonucu uçağa koymak | `aircraft.hpp` (`EnemyInfo` / `OwnInfo`) + `world.cpp` |
| Uçak tipinin sabitleri (menzil, radar) | `aircraft_profiles.hpp` |

**Önemli:** Kod, **fighter'ın (blue) içinde çalışıyormuş gibi** düşünülür. `fighter` biziz,
`bogey` (red) düşman. Sadece fighter sürülür, bogey başlangıçtaki komutla düz uçar.

---

## 2. Verileri okumak

Bütün veri `step` içinde `world_` değişkenindedir. Her adımın başında güncellenir.

```cpp
world_.sim_time_s      // simülasyon saati, saniye
world_.fighter         // biz   (Aircraft)
world_.bogey           // düşman (Aircraft)
```

### Bir uçağın içinde ne var

```cpp
const Aircraft& me = world_.fighter;   // kısaltma: her seferinde world_.fighter yazmamak için

// Ölçüm: konum ve hareket
me.kinematics.lat_deg           // enlem, derece
me.kinematics.lon_deg           // boylam, derece
me.kinematics.altitude_m        // irtifa, metre (deniz seviyesinden)
me.kinematics.heading_deg_180   // burun yönü, derece ±180 (0 kuzey, +90 doğu, -90 batı)
me.kinematics.pitch_deg         // yunuslama, derece (+ burun yukarı)
me.kinematics.roll_deg          // yatış, derece (+ sağ kanat aşağı)
me.kinematics.tas_mps           // gerçek hava hızı, m/s
me.kinematics.mach              // mach
me.kinematics.velocity_ned_mps[0]   // hız kuzey bileşeni, m/s  ([1] doğu, [2] aşağı)

// Kimlik ve sağlık
me.id        // "blue"
me.health    // 0..1, 1 sağlam

// Uçak tipinin sabitleri (aircraft_profiles.hpp tablosundan)
me.profile.max_g
me.profile.wez_max_range_m
me.profile.radar_range_m

// Hesaplananlar (world_.update() doldurur)
me.enemy.range_2d_m         // düşmana yatay mesafe, m
me.enemy.range_3d_m         // düşmana 3B mesafe (irtifa farkı dahil), m
me.enemy.bearing_deg_180    // düşmanın yönü, derece ±180 (kuzeye göre)
me.enemy.in_wez             // düşman füze menzilimde mi
me.own.ready_missiles       // atılmaya hazır füze sayım
me.own.missile_in_flight    // havadaki füze sayım
```

**Birim alan adında yazar:** `_m` metre, `_deg` derece, `_s` saniye, `_mps` m/s.
Yönler C++ içinde hep **±180**'dir (adında `_180` var).

### Örnek: basit kontroller

```cpp
const Aircraft& me = world_.fighter;
const Aircraft& enemy = world_.bogey;

if (me.kinematics.altitude_m < 2000.0)
{
    // çok alçaktayız
}

if (enemy.kinematics.altitude_m > me.kinematics.altitude_m)
{
    // düşman bizden yüksekte
}

if (world_.sim_time_s > 30.0)
{
    // ilk 30 saniye geçti
}
```

### Füzeleri okumak

Her uçağın füzeleri `missiles` listesindedir. Füzenin durumu `MissileState` ile tutulur:

| Değer | Anlamı | `kinematics` / `flight_time_s` |
|---|---|---|
| `MissileState::Ready` (0) | atılmadı, rampada | anlamsız, 0 |
| `MissileState::InFlight` (1) | havada | dolu |
| `MissileState::Ended` (2) | işi bitti, sonuç `outcome`'da (`"hit"` / `"lost"`) | son değerler |

```cpp
for (const Missile& m : world_.fighter.missiles)
{
    if (m.state == MissileState::InFlight)
    {
        // m.flight_time_s           atıştan beri geçen saniye
        // m.kinematics.altitude_m   füzenin irtifası
        // m.target_id               hedefi ("red")
    }

    if (m.state == MissileState::Ended && m.outcome == "hit")
    {
        // bu füze vurdu
    }
}
```

Düşmanın füzeleri de aynı şekilde `world_.bogey.missiles` içindedir. Bize gelen bir füze var
mı diye bakmak için aynı döngüyü onda kurarsın.

> Füzenin konumunu okumadan önce mutlaka `state`'e bak. `Ready` füzenin konumu 0'dır;
> okursan füzeyi 0,0 koordinatında sanırsın.

---

## 3. Uçağı komutlamak

Uçağa üç şey söylersin. Hepsi `FlightCommand` içindedir ([orders.hpp](orders.hpp)):

```cpp
fighter_command_.heading_deg_180   // gitmek istediğin yön, derece ±180
fighter_command_.altitude_m        // gitmek istediğin irtifa, metre
fighter_command_.throttle          // gaz, 0..1
```

Bunlar **hedef değerlerdir**, kumanda kolu hareketi değil. "90 dereceye dön" dersin, uçağın
kendi otopilotu (JSBSim) dönüşü yapar. Komut **kalıcıdır**: değiştirmediğin sürece her adım
aynısı gider.

`fighter_command_` her adımın başında Python'un en son uyguladığı komutla doldurulur.
Yani sadece değiştirmek istediğin alanı yazarsın, diğerleri olduğu gibi kalır.

### Örnekler (`step` içinde, 3. adımda)

```cpp
// Doğuya dön (irtifa ve gaz aynı kalır)
fighter_command_.heading_deg_180 = 90.0;

// Batıya dön
fighter_command_.heading_deg_180 = -90.0;

// 8000 metreye tırman
fighter_command_.altitude_m = 8000.0;

// Tam gaz
fighter_command_.throttle = 1.0;

// Şu anki yönümden 30 derece sağa dön: toplamı wrap_180 ile ±180'e getir
fighter_command_.heading_deg_180 = wrap_180(world_.fighter.kinematics.heading_deg_180 + 30.0);

// Burnu düşmana çevir
fighter_command_.heading_deg_180 = world_.fighter.enemy.bearing_deg_180;
```

`wrap_180` için dosyanın başına `#include "util_functions.hpp"` gerekir.

**Kurallar:**
- Yönü **±180** yaz. Python'a giderken 0..360'a otomatik çevrilir.
- Açıları toplarken ya da çıkarırken sonucu **`wrap_180(...)`** içine al. Örneğin 170 + 30 = 200
  geçersizdir; `wrap_180(200)` = -160 doğrudur.
- `throttle` 0 ile 1 arasında olmalı. Dışındaysa program anlaşılır bir hatayla durur.

---

## 4. Geometri: mesafe ve yön

### Hesap yapman gerekiyor mu?

**Mesafe için hayır, hazır. Yön için bir kere.** Geometri hesapları
[geometry.cpp](geometry.cpp)'dedir ve sonuçlar **her adım otomatik olarak** şuralara yazılır:

```
world_.fighter.enemy.range_2d_m        (bizden düşmana, yatay)
world_.fighter.enemy.range_3d_m        (bizden düşmana, irtifa farkı dahil)
world_.fighter.enemy.bearing_deg_180
world_.bogey.enemy.range_3d_m          (düşmandan bize) ...
```

| Fonksiyon | Durum |
|---|---|
| `Geometry::range_2d_m` | hazır: yatay mesafe, irtifa farkı yok sayılır |
| `Geometry::range_3d_m` | hazır: düz çizgi mesafe, irtifa farkı dahil |
| `Geometry::bearing_deg_180` | içi boş (TODO), şimdilik 0 |

Yani hesap **bir yerde yazılır** (`geometry.cpp`), **her yerde kullanılır**
(`me.enemy.range_3d_m`). Manevrada ya da `step` içinde tekrar hesaplamazsın.

**2B mi 3B mi?** Füze menzili (WEZ) ve radar için gerçek uzaklık olan `range_3d_m`. Haritadaki
uzaklık ya da "yatayda ne kadar yaklaştık" için `range_2d_m`. İrtifa farkı küçükse ikisi
neredeyse aynıdır.

### Nasıl bağlanıyor

```
geometry.cpp        Geometry::range_3d_m(own, target)   ← hesap burada
      ▲
world.cpp           fill_enemy_info: own.enemy.range_3d_m = Geometry::range_3d_m(...)
      ▲
autopilot.cpp       world_.update()  (her adım çağrılır)
      ▼
her yerde           world_.fighter.enemy.range_3d_m      ← sen okursun
```

### Hesap nasıl yapılıyor (düz dünya yaklaşımı)

Dünya iki uçak arasında düz kabul edilir: enlem farkı kuzey metresine, boylam farkı doğu
metresine çevrilir, sonra Pisagor. Hata yaklaşık %0,3 (90 km'de ~300 m); km'lik WEZ sınırları
için yeterli. Daha hassası gerekirse `util_functions.hpp`'deki `lla_to_ecef` kullanılır;
fonksiyonların dışarıdan görünüşü değişmez.

```cpp
// geometry.cpp (özet)
static const double EARTH_RADIUS_M = 6371000.0;   // m, ortalama Dünya yarıçapı

// own'dan target'a kuzey ve doğu yönünde kaç metre
static void horizontal_offset_m(const Kinematics& own, const Kinematics& target, double& north_m, double& east_m)
{
    double mean_lat_rad = (own.lat_deg + target.lat_deg) / 2.0 * DEG_TO_RAD;
    north_m = (target.lat_deg - own.lat_deg) * DEG_TO_RAD * EARTH_RADIUS_M;
    east_m = (target.lon_deg - own.lon_deg) * DEG_TO_RAD * EARTH_RADIUS_M * std::cos(mean_lat_rad);
}

double Geometry::range_2d_m(...)   // sqrt(north² + east²)
double Geometry::range_3d_m(...)   // sqrt(range_2d² + irtifa_farkı²)
```

`horizontal_offset_m`, sonucu iki değişkene birden yazar: `double& north_m` içindeki `&`,
çağıranın değişkenine doğrudan yazılacağını söyler.

### Yön (bearing) nasıl yazılır

Aynı `horizontal_offset_m` yönü de verir. `geometry.cpp`'deki TODO'nun yerine:

```cpp
double Geometry::bearing_deg_180(const Kinematics& own, const Kinematics& target)
{
    double north_m = 0;
    double east_m = 0;
    horizontal_offset_m(own, target, north_m, east_m);

    // atan2(doğu, kuzey): 0 kuzey, +90 doğu. Sonuç radyan → dereceye çevir.
    return wrap_180(std::atan2(east_m, north_m) * RAD_TO_DEG);
}
```

`std::sin`, `std::cos` ve `std::atan2` **radyan** ile çalışır. Dereceyi `DEG_TO_RAD` ile çarparak
radyana, sonucu `RAD_TO_DEG` ile çarparak dereceye çevirirsin.

### Yeni bir geometri değeri eklemek (örnek: ATA)

ATA (Antenna Train Angle), burnumla düşman arasındaki açıdır: 0 ise düşman tam önümde,
+90 ise sağımda. Dört adımda eklenir:

1. **Uçakta yerini aç.** `aircraft.hpp` → `EnemyInfo`:
   ```cpp
   double ata_deg_180 = 0;   // derece ±180, burnumla düşman arasındaki açı | Geometry::ata_deg_180
   ```
2. **Fonksiyonu tanıt.** `geometry.hpp` → `Geometry` sınıfına:
   ```cpp
   // derece ±180, own'un burnu ile target arasındaki açı; + sağda
   static double ata_deg_180(const Kinematics& own, const Kinematics& target);
   ```
3. **Hesabı yaz.** `geometry.cpp`:
   ```cpp
   double Geometry::ata_deg_180(const Kinematics& own, const Kinematics& target)
   {
       return wrap_180(bearing_deg_180(own, target) - own.heading_deg_180);
   }
   ```
4. **Her adım doldur.** `world.cpp` → `fill_enemy_info`:
   ```cpp
   own.enemy.ata_deg_180 = Geometry::ata_deg_180(own.kinematics, target.kinematics);
   ```

Artık her yerde `world_.fighter.enemy.ata_deg_180` kullanılabilir.

**Hangi dosya?**
- Sadece konum ve hareket gerekiyorsa (mesafe, açı) → `Geometry`, girdisi `Kinematics`.
- Uçağın tipine de bakıyorsa (füze menzili, radar) → `Weapons`, girdisi `Aircraft`.
  Örneğin `in_wez`: `own.enemy.range_3d_m` ile `own.profile.wez_min_range_m` / `wez_max_range_m`'yi
  karşılaştırır.

---

## 5. Füze atmak ve takip etmek

### Atmak

`step` içindeki 4. adımda `fighter_fire` değişkenini `true` yaparsın. O adım için atış emri
gider. Hedef her zaman bogey'dir.

```cpp
// 4. Atış
bool fighter_fire = false;

// Örnek 1: 10. saniyede ateş et
if (world_.sim_time_s > 10.0 && world_.sim_time_s < 11.0)
{
    fighter_fire = true;
}

// Örnek 2: düşman WEZ'imdeyse ateş et
if (world_.fighter.enemy.in_wez)
{
    fighter_fire = true;
}
```

Emir **tek seferliktir**: her adım yeniden karar verilir. `true` kaldığı sürece her adım
emir gider.

**Atış ne zaman gerçekten olur?** Kararı Python verir. Şu an:
- Hazır füze yoksa atmaz.
- Havada kendi füzen varken yenisini atmaz (şimdilik aynı anda bir füze).

Bu yüzden emri birkaç adım üst üste göndermek sorun yaratmaz: ilki atılır, gerisi füze havadayken
yok sayılır. C++ tarafı bu kısıtı bilmez ve kontrol etmez. Python ileride birden fazla füzeye
izin verirse C++'ta değişiklik gerekmez.

### Takip etmek

```cpp
for (const Missile& m : world_.fighter.missiles)
{
    if (m.state == MissileState::InFlight)
    {
        double missile_to_target_m = Geometry::range_3d_m(m.kinematics, world_.bogey.kinematics);
        // füze hedefe ne kadar yakın
    }
}
```

`Geometry` fonksiyonları `Kinematics` aldığı için füze için de aynen çalışır. Füzenin
konumu da bir `Kinematics`'tir.

Sonuç terminalde (`msl 3 hazir 1 havada`), kayıtta (`recordings/.../events.jsonl`) ve
görüntüleyicide görünür.

---

## 6. Manevralar

Manevra, "bu durumda nasıl uçarım?" sorusunun cevabını veren küçük bir sınıftır. Uçağı ve
mevcut komutu alır, yeni komutu döndürür. Hepsi [maneuver.hpp](maneuver.hpp)'de durur.

| Manevra | Ne yapar | Durum |
|---|---|---|
| `Hold` | Komutu değiştirmez, aynı yön, irtifa ve gazla devam eder | hazır |
| `Crank` | Düşmanı radar sınırında tutarak yana kaçar | içi boş (TODO) |

### Manevra seçmek

`step` içindeki 3. adımda istediğin satırı açarsın:

```cpp
// 3. Manevra
fighter_command_ = maneuvers_.hold.compute(world_.fighter, fighter_command_);
// fighter_command_ = maneuvers_.crank.compute(world_.fighter, fighter_command_);
```

Duruma göre seçmek de basit bir `if`'tir:

```cpp
if (world_.fighter.enemy.range_3d_m > 40000.0)
{
    fighter_command_ = maneuvers_.hold.compute(world_.fighter, fighter_command_);
}
else
{
    fighter_command_ = maneuvers_.crank.compute(world_.fighter, fighter_command_);
}
```

### Yeni manevra yazmak (örnek: Pursuit, düşmana doğru dön)

1. **Sınıfı tanıt.** `maneuver.hpp`:
   ```cpp
   // Burnu düşmana çevir; irtifa ve gaz aynı.
   class Pursuit : public Maneuver
   {
   public:
       FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;
   };
   ```
2. **Gövdesini yaz.** `maneuver.cpp`:
   ```cpp
   FlightCommand Pursuit::compute(const Aircraft& fighter, const FlightCommand& current)
   {
       FlightCommand command = current;                           // önce mevcut komutu kopyala
       command.heading_deg_180 = fighter.enemy.bearing_deg_180;   // sadece yönü değiştir
       return command;
   }
   ```
3. **Listeye ekle.** `maneuver.hpp` → `struct Maneuvers`:
   ```cpp
   Pursuit pursuit;
   ```
4. **Kullan.** `autopilot.cpp` → `step`:
   ```cpp
   fighter_command_ = maneuvers_.pursuit.compute(world_.fighter, fighter_command_);
   ```

**Kalıp hep aynı:** `current`'ı kopyala, sadece ilgilendiğin alanı değiştir, döndür.
Böylece manevra yönü değiştirirken irtifayı ve gazı yanlışlıkla sıfırlamaz.

### Manevranın hafızası

Manevra nesneleri program boyunca bir kez oluşur, adımlar arasında yaşar. Bir manevranın
önceki adımdan bir şey hatırlaması gerekiyorsa (örneğin crank'te hangi tarafa döndüğü),
bunu sınıfın `private` kısmına yazarsın:

```cpp
// maneuver.hpp
class Crank : public Maneuver
{
public:
    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    bool turn_right_ = true;     // hangi tarafa crank yapıyoruz; adımlar arasında korunur
    double offset_deg_ = 50.0;   // düşmanı burnun ne kadar yanında tutacağız
};
```

```cpp
// maneuver.cpp
FlightCommand Crank::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;

    double side = turn_right_ ? 1.0 : -1.0;   // sağa crank: burun düşmanın sağına, düşman solumuzda kalır
    command.heading_deg_180 = wrap_180(fighter.enemy.bearing_deg_180 + side * offset_deg_);

    return command;
}
```

Sınıf üyelerinin adı `_` ile biter (`turn_right_`). Böylece okurken yerel değişkenden ayırt edilir.

---

## 7. Hepsi bir arada: basit bir taktik

Aşağıdaki örnek `Autopilot::step`'in 3. ve 4. adımları için basit bir taktik:
uzaktayken düşmana dön, menzile girince ateş et, füze havadayken crank yap.
Geometri hesabı (bölüm 4), `in_wez` (bölüm 4 son kısım) ve `Pursuit` (bölüm 6) yazılmış varsayılır.

```cpp
// 3. Manevra
const Aircraft& me = world_.fighter;

if (me.own.missile_in_flight > 0)
{
    // füzem havada: düşmanı radarda tutarak yana kaç
    fighter_command_ = maneuvers_.crank.compute(me, fighter_command_);
}
else
{
    // füzem yok: düşmana doğru dön
    fighter_command_ = maneuvers_.pursuit.compute(me, fighter_command_);
}

// 4. Atış
bool fighter_fire = false;
if (me.enemy.in_wez && me.own.ready_missiles > 0)
{
    fighter_fire = true;
}

// 5. Komut (değiştirme)
return CommandWriter::write(state.tick, fighter_command_, bogey_command_, fighter_fire);
```

Taktik büyüdükçe bu `if`'ler ayrı bir karar sınıfına taşınabilir. Manevralar ve hesaplar
olduğu gibi kalır; sadece "hangisini seçeyim" kısmı yer değiştirir.

---

## 8. Değişken eklemek

Değişkeni nereye ekleyeceğin, iki soruya bağlı: **değer nereden geliyor** ve **ne kadar
yaşaması gerekiyor**.

| Değişken ne? | Örnek | Nereye |
|---|---|---|
| A. Sadece bu adımda lazım, geçici | iki irtifanın farkı | `step` içinde yerel değişken |
| B. Adımlar arasında hatırlanmalı | son atış zamanı | `Autopilot` sınıfına üye (`autopilot.hpp`) |
| C. Her adım hesaplanan, bir uçağa ait bilgi | enerji yüksekliği, ATA | `OwnInfo` / `EnemyInfo` + `world.cpp` |
| D. Uçak tipinin değişmeyen özelliği | füze hızı, radar menzili | `AircraftProfile` + tablo |
| E. Simülasyondan gelen yeni ölçüm | g yükü | Python + `aircraft.hpp` + `state_reader.cpp` |
| F. Hiç değişmeyen sabit sayı | yer çekimi ivmesi | Kullanıldığı `.cpp`'nin başına `static const` |

### A. Geçici değişken: `step` içinde

Her adım yeniden oluşur, adım bitince kaybolur. En basit hâli budur.

```cpp
// autopilot.cpp → step
double altitude_diff_m = world_.bogey.kinematics.altitude_m - world_.fighter.kinematics.altitude_m;

if (altitude_diff_m > 1000.0)
{
    fighter_command_.altitude_m = world_.bogey.kinematics.altitude_m;   // düşmanın irtifasına çık
}
```

### B. Adımlar arasında hatırlanan değişken: `Autopilot` üyesi

Yerel değişken her adım sıfırdan başlar. Bir şeyi önceki adımlardan hatırlaman gerekiyorsa
(ne zaman ateş ettim, kaç kez döndüm), sınıfın üyesi yaparsın. Üyeler program boyunca yaşar.

1. `autopilot.hpp` → `private` kısmına, başlangıç değeriyle:
   ```cpp
   double last_fire_s_ = -1000.0;   // s, son atış emrinin zamanı; başta "çok önce"
   ```
2. `autopilot.cpp` → `step` içinde kullan:
   ```cpp
   // 4. Atış: son atıştan en az 20 saniye geçtiyse
   bool fighter_fire = false;
   if (world_.fighter.enemy.in_wez && world_.sim_time_s - last_fire_s_ > 20.0)
   {
       fighter_fire = true;
       last_fire_s_ = world_.sim_time_s;   // hatırla
   }
   ```

Üye adları `_` ile biter. Aynı fikir manevralarda da var (bölüm 6, "Manevranın hafızası"):
sadece o manevranın hatırlaması gereken şey manevranın içine yazılır.

### C. Hesaplanan uçak bilgisi: `OwnInfo` / `EnemyInfo`

Her adım ölçümden hesaplanan ve birçok yerde kullanılacak bir değerse, bir kez hesaplanıp
uçağın içine konur. Böylece her yerde `me.own.xxx` ya da `me.enemy.xxx` diye okunur.

- **Sadece kendi uçağıma bağlıysa** → `OwnInfo`, hesap `world.cpp` → `compute_own_info`.
- **Karşı uçağa göreyse** → `EnemyInfo`, hesap `world.cpp` → `fill_enemy_info`
  (ATA örneği: bölüm 4).

Örnek: enerji yüksekliği (irtifa + hızın irtifa karşılığı), `OwnInfo`'ya.

1. `aircraft.hpp` → `OwnInfo`:
   ```cpp
   double energy_height_m = 0;   // m, irtifa + v²/2g | compute_own_info
   ```
2. `world.cpp` → `compute_own_info` (dosyanın başına `static const double GRAVITY_MPS2 = 9.81;`):
   ```cpp
   double v = own.kinematics.tas_mps;
   info.energy_height_m = own.kinematics.altitude_m + v * v / (2.0 * GRAVITY_MPS2);
   ```
3. Kullan: `world_.fighter.own.energy_height_m`

### D. Uçak tipinin sabiti: `AircraftProfile`

Simülasyondan gelmeyen, uçak tipine göre değişen ama uçuş boyunca sabit kalan değerler.

1. `aircraft_profiles.hpp` → `struct AircraftProfile`'a alanı ekle:
   ```cpp
   double missile_speed_mps = 0;   // m/s, füzenin ortalama hızı
   ```
2. Aynı dosyadaki tabloda **her satıra**, struct'taki sırayla değerini ekle:
   ```cpp
   //                   name               mach   g     wez_max  wez_min  radar   gimbal  msl_speed
   {"F16",            {"F-16",             2.05,  9.0,  31000,   2000,    80000,  60,     1000}},
   ```
3. Kullan: `world_.fighter.profile.missile_speed_mps`

Tablodaki değerler sırayla alanlara yazılır. Yeni alanı struct'ın sonuna eklediysen değeri de
satırın sonuna yaz.

### E. Simülasyondan gelen yeni ölçüm

Değer C++'a hiç gelmiyorsa önce Python'un göndermesi gerekir. Üç dosya değişir.
Örnek: g yükü (JSBSim'de `accelerations/Nz`).

1. **Python gönderir.** `src/sim/simulation.py` → `_kinematics`:
   ```python
   g_load=float(model.fdm["accelerations/Nz"]),
   ```
2. **C++'ta yerini aç.** `aircraft.hpp` → `Kinematics`, birim ve kaynak yorumuyla:
   ```cpp
   double g_load = 0;   // g, normal yük faktörü | g_load | accelerations/Nz
   ```
3. **JSON'dan oku.** `state_reader.cpp` → `read_kinematics`, türü açıkça yazarak:
   ```cpp
   k.g_load = j.at("g_load").get<double>();   // g
   ```

Artık `world_.fighter.kinematics.g_load` kullanılabilir. `Kinematics`'e eklediğin için atılmış
füzelerde de aynı alan dolar. Python füzeler için de aynı `_kinematics`'i kullanıyor.

Uçağa özgü, konumla ilgisi olmayan bir alan (örneğin yakıt) `Kinematics` yerine `Aircraft`'a,
okuması da `read_aircraft`'a yazılır.

### F. Sabit sayı

Değeri hiç değişmeyen ve sadece bir dosyada kullanılan sayılar, o `.cpp`'nin başına yazılır:

```cpp
static const double GRAVITY_MPS2 = 9.81;   // m/s², yer çekimi ivmesi
```

Birden fazla dosyada lazımsa `util_functions.hpp`'deki `PI` gibi bir `.hpp`'ye konur.

**Her durumda:** adın sonuna birimi yaz (`_m`, `_s`, `_deg`, `_mps`), yanına birim ve
kaynağı söyleyen kısa bir yorum koy, başlangıç değeri ver (`= 0`).

---

## 9. Terminale bir şey yazdırmak

Saniyede bir satır `autopilot.cpp` → `print_aircraft` içinde yazılır. Yeni bir değeri görmek
için oraya ekle:

```cpp
std::cout << a.id
          << ...
          << "  ata " << a.enemy.ata_deg_180;   // yeni satır
```

Hızlı bir deneme için `step` içinde de yazdırabilirsin. Ama her adım yazarsan terminal
saniyede 12 satırla dolar; mevcut "saniyede bir" kontrolünün içine koy.

---

## 10. Sık yapılan hatalar

| Belirti | Sebep | Çözüm |
|---|---|---|
| Uçak ters yöne ya da garip dönüyor | Açı toplamı ±180 dışına çıktı | Sonucu `wrap_180(...)` içine al |
| `sin`/`cos` sonuçları saçma | Derece verildi, fonksiyon radyan bekliyor | `* DEG_TO_RAD` ile çevir |
| Uçak irtifa kaybediyor ya da gazı kesiyor | Manevrada yeni `FlightCommand` sıfırdan oluşturuldu | `FlightCommand command = current;` ile başla |
| Füze 0,0 koordinatında görünüyor | `Ready` füzenin konumu okundu | Önce `m.state`'e bak |
| `brg 0.0` | Yön fonksiyonu henüz boş | Bölüm 4 |
| Program `throttle 0..1 disinda` diyerek duruyor | Gaz 1'den büyük ya da 0'dan küçük | 0..1 aralığında tut |
| `'X' has not been declared` | Dosya kaydedilmedi ya da `#include` eksik | `Ctrl+K S`; gereken `.hpp`'yi ekle |
| JSON ile ilgili `get` hatası | JSON'dan değer okurken tür yazılmadı | `j.at("alan").get<double>()` biçiminde yaz (sadece `state_reader.cpp`'de) |

Bir değişiklikten sonra: dosyaları kaydet, derle (`cmake --build src/otopilot/build`),
önce Python'u sonra `main.exe`'yi çalıştır. Ayrıntısı
[docs/CALISTIRMA_KILAVUZU.md](../../docs/CALISTIRMA_KILAVUZU.md)'de.
