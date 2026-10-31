# C++ otopilot mimarisi

C++ uygulaması ölçümden komuta giden dış kontrol adımını yürütür. Fizik modelini ilerletmez; yön, irtifa, gaz ve g sınırı hedefleri ile tek adımlık atış emirleri üretir. İki uçağın kararları ayrı otopilot nesnelerinde yaşar, ama aynı `World` verisini kullanır.

Kod kuralları (katmanlar, isimlendirme, birimler, "değer yok" durumu): [KOD_STANDARTLARI.md](../../src/otopilot/KOD_STANDARTLARI.md). Pratik örnekler: [KULLANIM.md](../../src/otopilot/KULLANIM.md).

## Katmanlar

| Klasör | Görev | Yazdığı veri |
|---|---|---|
| `io/` | JSON ↔ C++ | `Aircraft`'ın ölçüm alanları, `command` (Python'un son uyguladığı) |
| `model/` | Veri tipleri, `World::update` | `enemy_id`, `picture`, füzelerin `target` bilgisi |
| `calculations/` | Saf hesap (`geometry::`, `turn::`, `engagement::`, `missile_geometry::`) | Hiçbir şey; sonuç döndürür |
| `control/` | Karar ve manevra | `command`, `phase`, atış emri |
| `common/` | Birim, açı, NED vektörü, LLA/ECEF | — |

Hesap fonksiyonları `picture`'ı okumaz; ihtiyaç duydukları ara sonuçları parametre olarak alır. Hesap sırası yalnız `World::update` içinde durur. Karar ve manevra kodu ise hesap yapmaz, yalnız `picture`'ı okur.

## Nesne sahipliği

```mermaid
classDiagram
    class Sim {
        World world_
        OtopilotFighter otopilot_fighter_
        OtopilotHostile otopilot_hostile_
        Message step(Message state)
    }
    class World {
        double sim_time_s
        Aircraft fighter
        Aircraft hostile
        update()
    }
    class Aircraft {
        Kinematics kinematics
        AircraftProfile profile
        string enemy_id
        Picture picture
        Weapons weapons
        FlightCommand command
        Phase phase
    }
    class Weapons {
        Missile[] missiles
        bool has_launch_order
        Wez wez
        double nez_nm
        optional f_pole_nm
        bool enemy_in_wez
        bool enemy_in_nez
        fire(MissileProfile) bool
    }
    class OtopilotFighter {
        FighterState state_
        Maneuvers maneuvers_
        step(Aircraft& own, Aircraft enemy)
        go_to(FighterState, Maneuver&, own)
    }
    class OtopilotHostile {
        HostileState state_
        Maneuvers maneuvers_
        step(Aircraft& own, Aircraft enemy)
        go_to(HostileState, Maneuver&, own)
    }
    class Maneuver {
        phase()*
        enter(own)
        update(own)* FlightCommand
        status(own) ManeuverStatus
    }
    Sim *-- World
    Sim *-- OtopilotFighter
    Sim *-- OtopilotHostile
    World *-- Aircraft
    Aircraft *-- Weapons
    OtopilotFighter *-- Maneuvers
    OtopilotHostile *-- Maneuvers
    Maneuvers *-- Maneuver
```

`main.cpp` bir `Connection` ve bir `Sim` oluşturur; ikisi de program boyunca yaşar. Her ölçüm için aynı `Sim::step()` çağrıldığından otopilotların durumu ve manevraların hafızası korunur.

## Adımın sırası

```cpp
StateReader::read_world(state.data, world_);              // ölçüm + own.command (Python'un son uyguladığı)
world_.update();                                          // düşman ataması, füze bilgisi, picture
otopilot_fighter_.step(world_.fighter, world_.hostile);   // command, phase, atış emri
otopilot_hostile_.step(world_.hostile, world_.fighter);
return CommandWriter::write(state.tick, world_);
```

## Veri

| Yapı | İçerik | Yazan |
|---|---|---|
| `Kinematics` | konum, yönelim, TAS, Mach, NED hız | StateReader |
| `AircraftProfile` | ana silah (`main_weapon`), yük, radar, out g'si ve payı | StateReader (tip değişince) |
| `Weapons` | füze slotları, atış emri; ana silahımın WEZ'i, NEZ'i, F-pole'u, düşman WEZ/NEZ'imde mi | StateReader, `weapons.fire`, World::update |
| `Missile` | slot/profil, atış bilgisi, Python kimliği/durumu, kinematik, `target` (hedefe göre) | StateReader, `weapons.fire`, World::update |
| `Picture` | atanmış düşmana göre geometri, out planı, kaçış: R_cold, NEZ, MAR, DOR, `threat` | World::update |
| `ThreatInfo` | bana gelen en acil düşman füzesinin bana göre durumu | World::update |
| `FlightCommand` | yön, irtifa, gaz, isteğe bağlı g sınırı; setter'lar aralığı korur | StateReader, otopilot |
| `Phase` | kayda çıkan aşama: `0 Hold`, `1 Fire`, `2 Out`, `3 In`, `4 Crank`, `5 Beam` | otopilot (aktif manevradan) |

"Olmayabilen" sayılar `std::optional` tipindedir: tehdit (bana gelen füze yoksa `threat`), füze yetişemiyorsa `f_pole_nm`. WEZ, R_cold, NEZ, MAR ve DOR her uçağın profilindeki `main_weapon`'dan hesaplandığı için her zaman vardır. Kayda olmayan değerler 0 olarak yazılır.

## World hesapları

`World::update()` şu sırayla çalışır:

1. **Düşman ataması.** 1v1'de sabit: blue → red, red → blue. Çok uçaklı yapıya geçilirse yalnız burası değişir.
2. **Füze bilgileri.** Her uçağın havadaki füzeleri için hedefe göre mesafe, kapanma ve pitbull hesaplanır. Pitbull bir kez true olunca öyle kalır.
3. **Picture.** Her iki uçak için:
   - geometri (`geometry::`)
   - out planı (`engagement::out_plan` → `turn::to_heading`)
   - düşmanın ana silahının (`enemy.profile.main_weapon`) R_cold uyumuyla R_cold, NEZ ve MAR
   - DOR: MAR + düşmanın 180°'lik recommit dönüşü boyunca yaklaştığı yol
   - tehdit: düşmanın bana gelen füzelerinden bana en yakın olanı
4. **Weapons.** Her iki uçak için kendi ana silahıyla (`own.profile.main_weapon`):
   - WEZ ve `enemy_in_wez`
   - F-pole
   - NEZ: düşmanın gözünden kaçış NEZ'i (`engagement::nez_nm(enemy, own)`: düşmanın benden out planı ve füzemin R_cold uyumu); `enemy_in_nez` = WEZ iç sınırı < menzil ≤ NEZ. 1v1'de bu, düşmanın kendi `picture.nez_nm`'ine eşittir.
5. **`me_in_enemy_wez`.** Karşı tarafın `weapons.enemy_in_wez` sonucundan kopyalanır.

## Dönüş ve out planı

`turn::to_heading(own, hedef_yön, g, bitiş_hızı)` kısa taraftan, sabit g ile seviye dönüşü hesaplar ve sonucu `TurnResult` olarak döndürür. Hız dönüş boyunca doğrusal değişir kabul edilir; yarıçap ve süre ortalama hızla hesaplanır.

```text
r  = V² / (g · √(n² − 1))
Δt = |Δψ| · r / V
yer değişimi (s = +1 sağ, −1 sol):  kuzey = s·r·(sin ψ1 − sin ψ0),  doğu = s·r·(cos ψ0 − cos ψ1)
```

Out planı, düşmanın bearing'i + 180°'ye yapılan dönüştür. Kaçış hızı şimdilik şu anki TAS kabul edilir. Dönüşte düşmana yaklaşma = R − |LOS − yer değişimi|. Commit ve recommit hesapları da aynı fonksiyon üzerine kurulur.

## Manevralar ve karar

Manevra yaşam döngüsü: `enter(own)` girişte bir kez, `update(own)` her step, `status(own)` → `Running` / `Done`.

| Manevra | Faz | Davranış | Bitişi |
|---|---|---|---|
| `Hold` | Hold | ölçülen yön ve irtifayı hedef yapar, gaz aynı | sürekli |
| `Fire` | Fire | komutu değiştirmez | sürekli |
| `Out` | Out | bearing − 180, girişte kilitlenen alçalma, tam gaz, out g'si | \|ATA\| > 177° → Done |
| `In` | In | yön = bearing | sürekli |
| `Crank` | Crank | gimbal − 5° hedef ATA'ya P kontrolü; `in_radar_sight()` sorgusu (düşman radar konisinde mi, \|ATA\| < gimbal). Taraf `enter()`'da `crank_side_for` ile seçilir (`side()` ile okunur): düşman hangi taraftaysa o tarafta kalır | sürekli |
| `Beam` | Beam | bearing ∓ 90; taraf burnuma göre seçilir (düşman sağımdaysa sola, solumdaysa sağa), dönüş hep 90°'den kısa | sürekli |

`OtopilotFighter` ve `OtopilotHostile` birer durum makinesidir; ikisi de `step` içinde tek bir switch kullanır. Her durum bir bloktur: önce o durumun manevrası `fly(own, manevra)` ile uçulur (fazı da manevradan alır), sonra çıkış koşullarına bakılır. Geçiş `go_to(yeni_durum, yeni_manevra, own)` ile yapılır; yeni manevranın `enter()`'ı o anda çağrılır, kendisi bir sonraki step'te uçulur. Başlangıç durumunun manevrası (In) `enter()` kullanmaz; başlangıç `enter()` kullanan bir manevraya değişirse ilk step'te çağrılması gerekir. Geçiş koşulları `control/conditions.hpp` içinde isimli fonksiyonlardır; karar yapısı ileride değişse de manevralar ve koşullar aynı kalır.

Mevcut taktik: şov senaryosu. MAR'ın doğru çalıştığını tek uçuşta gösterir. Çalıştırma (yaml'a dokunmadan):

```powershell
python src/sim/sim_main.py --name show --range 40 --altitude 6000
```

6 km, R_cold uyumunun geçerli aralığının (4–10 km) ortasıdır. Kayıt: `recordings/show`.

| Fighter | Geçiş |
|---|---|
| Commit (In) | düşman WEZ'imde → Fire; atmadan MAR'a girdim → Out |
| Fire | AIM-120C atış emri, crank tarafı seçilir → Crank |
| Crank | MAR'a girdim → Out |
| Out | Out bitince gaz 0.45 → HoldAfterOut |
| HoldAfterOut | Hold |

| Hostile | Geçiş |
|---|---|
| Commit (In) | fighter kendi MAR'ına girdi (`inside_enemy_mar`) → Fire |
| Fire | AIM-120C atış emri → Out |
| Out | Out bitince gaz 0.45 → HoldAfterOut |
| HoldAfterOut | Hold |

`recordings/show` uçuşunun akışı:

| t (s) | Olay |
|---|---|
| 0 | İki F-16 40 NM'den birbirine doğru (In) |
| 65 | Fighter MAR'a 5 NM kala, 17.9 NM'den atar (MAR 12.9), crank'e girer |
| 86 | Fighter MAR'a girer (11.64 ≤ 11.67), out yapar; hostile aynı step'te atar ve out yapar |
| 100 | Fighter'ın out'u biter |
| 107 | Hostile'ın out'u biter |
| 127 | Fighter'ın füzesi hostile'ı vurur (292 m, isabet yarıçapı 300 m) |
| 141 | Hostile'ın füzesi fighter'a 5.5 km kala Mach 1'in altına düşer, kaybolur: MAR'da dönen fighter kurtuldu |

Bilinmesi gerekenler:
- BVRGym füzesi 6 km'de burun buruna, manevra yapmayan hedefi yalnız ~12 NM'ye kadar vurur; daha uzakta tırmanıp dalma (loft) sonrası 0.3–1.9 km ile ıskalar. Dönen hedefi daha yakından da ıskalayabilir.
- Gelen füzeye karşı "kalan süre" (mesafe / kapanma) füze yavaşladıkça ve biz döndükçe zıpladığı için kaldırıldı. Yerine kaçış payı (`escape_margin_nm`) gelecek.

## Füze slotları ve atış yaşam döngüsü

- **Slotların kurulması:** İlk ölçümde her uçağın mühimmat sayısı Python mesajından sayılır, slot tipleri `AircraftProfile.loadout`'un başından alınır. Python'daki sayı yük listesini aşarsa program durur.
- **Atış emri:** `own.weapons.fire(AIM120C)` o profildeki ilk atılmamış slotu işaretler, `fire_order` verir ve `weapons.has_launch_order` bayrağını true yapar. Henüz Python kimliği eşleşmemiş bir önceki atış varsa yeni atış istemez.
- **Eşleştirme:** `CommandWriter` bayrağı `{id, target_id}` emrine çevirir. Sonraki ölçümde `StateReader` Python füzesini önce bilinen kimlikten, yeni bir kimlikse henüz eşleşmemiş en eski C++ atışından bulur.
- **Sorgular:**
  - `last_fired()`: son atılan füze
  - `weapons.active_missile()`: havadaki son füze
  - `weapons.has_active_missile_against(target_id)`
  - `picture.threat`: bana gelen en acil düşman füzesi

## Giriş/çıkış sınırı

- **JSON'u bilen sınıflar:** Yalnız `StateReader`, `CommandWriter` ve `CalculationWriter`.
- **Kayıt sütun adları:** Şimdilik eski adlarıyla korunuyor: `rate_of_closure_kt` (= `closure_kt`), `enemy_closing_kt` (= `enemy_closure_kt`), `stern_wez_nm` (= `r_cold_nm`).
- **Bağlantı:** `Connection` Windows TCP ayrıntılarını saklar. Mesaj sırası `state(t) → command(t)` şeklindedir.

## Hesapların sınırları

- **Geometri:** Ortalama Dünya yarıçaplı yerel düz dünya yaklaşımı kullanılır.
- **WEZ:** İlk slotun profiline dayanır; düşmanın bana doğru hız bileşeniyle uzar:

```text
max_nm = missile.max_range_nm + enemy_closure_kt × (missile.max_range_nm / missile.speed_kt)
```

- **R_cold:** BVRGym füzesine karşı yapılan kaçış taramasından elde edilen üstel bir uyumdur ([r_cold_formulu.md](../../src/otopilot/calculations/r_cold_formulu.md)).
- **Profiller:** Profil seçimi Python fiziğini değiştirmez. Birim ve koordinat ayrıntıları [veri referansındadır](../veri_info/SIMULASYON_VERI_REFERANSI.md).
