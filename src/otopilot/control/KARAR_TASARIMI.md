# Karar tasarımı

**Durum:** tasarım. Kodda henüz yok; bugünkü `otopilot.cpp` / `decisions.cpp` eski akıştır.
Kararlar 2026-11-07 tartışmasında verildi. Ertelenenler en sonda.

## 1. Yapı

- İskelet **durum makinesi (FSM)**. Akış döngülü (Out → Recommit → Commit → ...), ağaç değil.
- Her geçiş bir **karar fonksiyonu**: `Decision{choice, reason}` döndürür, `own` ve `mission`'ı okur, akışı değiştirmez.
- Karar fonksiyonlarının içi önce **sıralı kurallar**. Seçeneği çok olan düğümler (TRADE, REASSESS) oturunca puanlamaya geçilebilir; arayüz değişmez.
- **REASSESS** anlık bir düğümdür, uçulmaz. Her taahhüt (savunma, destek, kovalama) bittiğinde buraya dönülür; aynı alt ağaçlar her dalda kopyalanmaz.
- Atış faz değil, geçişe bağlı **eylemdir** (`Shot`: emir kabul edilene kadar tekrar denenir, sonra SUPPORT).

### Her step

```
1. Kesinti: ABORT koşulu         → ABORT          (ABORT dışındaki her fazda)
2. Kesinti: tehdit               → karşı atış / TRADE / DEFEND
                                   (COMMIT, SUPPORT, OUT, COLD, RECOMMIT, CHASE'te)
3. Fazın kendi geçişi
4. Fazın manevrası
```

## 2. Fazlar

| Faz | Anlamı | Manevra |
|---|---|---|
| COMMIT | Düşmana git, atış noktasını bekle | `commit` |
| SUPPORT | Füzem havada, tehdit yok | düşman hot: `crank`, cold: kovalama |
| TRADE | İki füze havada, benimki pitbull değil | `crank` |
| DEFEND | Tehdit var, destek yok | `beam` → `drag` → `notch` (yalnız acilleşir) |
| OUT | Düşmanı arkaya al | `out` |
| COLD | Cold ops: MRR'ye açıl, recommit anını bekle | `out` yönü, art yakıcı; bekleme: `hold` |
| RECOMMIT | Hot'a dön | `recommit` |
| CHASE | İmha, düşman kaçıyor | pure pursuit, art yakıcı |
| ABORT | Kalıcı çıkış, recommit yok | `out`, Done sonrası art yakıcı |
| REASSESS | Anlık düğüm | — |

**Aspect bantları:** düşman cold = TA > 110°, hot = TA ≤ 70°. Aradaki beam bandı histerezis görevi görür.
**Histerezis:** zamanlayıcı yok. Bant + geri dönüşsüz kararlar (destek bırakmak, ABORT, "vazgeçtim" bayrağı) + yalnız acilleşen savunma. Testte titreme görülürse sim zamanı otopilota taşınıp minimum faz süresi eklenir.

## 3. Destek kuralı (varsayım)

Pitbull'dan önce atan uçak desteği bırakırsa (hedef gimbal dışı) ya da vurulursa füze güdümü kaybeder. İki taraf için geçerli.
**BVRGym'de yok:** `missiles.py` PN güdümü hedefin gerçek konumunu okur. Kural BVRGym'e ayrıca eklenecek; tasarım varmış gibi kurulur.

Füze güdümlü mü = `is_pitbull` **veya** atan uçağın ATA'sı gimbal içinde.

## 4. Kesintiler

**ABORT** (kalıcı):
1. Füzem kalmadı ve havada füzem yok
2. MAR ihlali
3. Görev ayarı: SUPPORT bitince OUT yerine ABORT (L&L)

ABORT'ta tehdit gelirse savunma yapılmaz, Out'a devam edilir.

**Tehdit** (yetişebilen en yakın düşman füzesi):
1. Tehdit yok ya da yetişemiyor → devam
2. Havada füzem yok ve karşı atış şartı var → karşı atış → SUPPORT (sonraki step TRADE'e düşer)
   Güvenli: yok · Dengeli: düşman TR'mde · Riskli: düşman WEZ'imde
3. Füzem havada ve pitbull değil → TRADE
4. Diğer → DEFEND

## 5. Düğümler

### COMMIT
1. Düşman cold → REASSESS
2. WEZ içi ve menzil ≤ TR → ateş (Pk ikinci aşamaya kadar sabit nokta)
3. TR çözümü yok, WEZ içi ve menzil ≤ DOR → ateş

### SUPPORT
1. Füzem bitti → REASSESS
2. İmha + Riskli, füzem var, havada < 2, menzil ≤ MSR → 2. atış; en yeni füze desteklenir
3. En yeni füzem pitbull:
   - düşman cold → REASSESS
   - Riskli, havada tek füze, füzem var → MSR'ye kadar devam
   - diğer → OUT (görev ayarı varsa ABORT)
4. Riskli değil ve menzil ≤ DOR → OUT (pitbull olmasa da bırakılır, füze kaybedilir)

### TRADE

| Süre | Anlamı |
|---|---|
| `t_pb_ben`, `t_pb_tehdit` | Füzemin / tehdidin pitbull'a kalan süresi |
| `t_vur_ben`, `t_vur_tehdit` | Füzemin / tehdidin yetişme süresi (`catch_time_left_s`) |
| `t_kritik` | `t_vur_tehdit − (out.turn.time_s + pay)`: crank'i en geç bırakma anı |
| `t_kritik_düşman` | `t_vur_ben − düşmanın savunma süresi`; düşmanınki kendi gözümden: `out_plan(enemy, own)` |

1. Tehdit bitti ya da güdümsüz kaldı → SUPPORT
2. Füzem bitti → DEFEND
3. Füzem pitbull → DEFEND (tipi DEFEND kuralı seçer)
4. **A:** `t_pb_ben ≤ t_kritik` → crank'e devam
5. **B2:** `t_kritik_düşman < t_pb_tehdit` ve `t_kritik_düşman + pay < t_kritik` → crank'e devam (düşman önce kırmak zorunda)
6. **B1:** `t_vur_ben + pay < t_pb_tehdit` → crank'e devam (füzem tehdit pitbull olmadan vurur)
7. Diğer → desteği hemen bırak, DEFEND

Risk: Güvenli A · Dengeli A + B2 · Riskli A + B2 + B1. Tehdit zaten pitbull ise yalnız A.

### DEFEND

Metrik **kaçış payı (NM)**: şimdi Out + Drag yapsam füze ölene kadar aramda kalan mesafe.

```
t_d  = out.turn.time_s,  v_e = out.escape_kt
v_m  = tehdidin ölçülen TAS'ı,  a = profil slowing_accel_kt_s (−18)
dönüş yolu  = (şimdiki kapanma + (v_m + a·t_d − v_e)) / 2 · t_d / 3600
v_m2  = v_m + a·t_d
v_son = max(v_e, Mach 1)                 Mach 1 = tehdit TAS / tehdit Mach
t_son = (v_m2 − v_son) / |a|
x     = ((v_m2 − v_e)·t_son + a·t_son²/2) / 3600
pay   = (füze–ben mesafesi − patlama yarıçapı − dönüş yolu) − x
```

1. Tehdit bitti ya da güdümsüz kaldı → REASSESS
2. pay > BEAM_PAYI ve Güvenli değil → Beam
3. 0 ≤ pay ≤ BEAM_PAYI → Drag (Out)
4. pay < 0 → Drag; `catch_time_left ≤ notch dönüşü + pay` olunca Notch

Notch yalnız son çare (simde doppler yok, füze yalnız kinematik yenilir).
Sabit ivme varsayımı: hız/irtifa/manevra etkisini görmez, hata çoğunlukla güvenli tarafta.

### REASSESS
1. Füzem kalmadı ve havada yok → ABORT
2. Düşman cold → İmha: CHASE · Püskürt: COLD
3. Ben cold → COLD
4. Menzil ≤ DOR → OUT
5. Diğer → RECOMMIT

### OUT
- Dönüş bitti → COLD

### COLD
- Gaz art yakıcı.
- Düşman hot, menzil ≥ MRR, füzem var ve recommit kaçış payı ≥ 0 → RECOMMIT
  (recommit kaçış payı: havada bana gelen füze varsa aynı formül, ben füzeye dönmüş kabul edilir)
- Düşman cold: İmha ve vazgeçmediysem → CHASE; Püskürt ya da vazgeçtiysem → menzil ≥ MRR ise `hold`, değilse kaçışa devam
- Düşman hot kovalıyor, MRR'ye açılamıyorum → kaçmaya devam (DEFEND ya da MAR→ABORT karar verir)
- Son füzem havadaysa recommit yok; füze bitince ABORT tetiklenir. Pitbull füzemin sonucu beklenmez.

### RECOMMIT
1. Dönüş bitti → COMMIT
2. Dönerken menzil ≤ DOR → OUT

### CHASE
1. Düşman hot → menzil ≤ DOR ise OUT, değilse COMMIT
2. Menzil ≤ R_cold'um ve füzem var → ateş → SUPPORT (kovalama manevrası sürer)
3. Burun düşmanda (|ATA| ≤ 30°) ve kapanma ≤ 0 → vazgeç: "vazgeçtim" bayrağı, COLD
4. Diğer → devam

"Vazgeçtim" bayrağı düşman hot'a dönünce sıfırlanır.

## 6. Gereken ek kod

### Hesaplar

| Ne | Nereye |
|---|---|
| `pitbull_time_left_s(missile)`: yol = menzil − pitbull, `accel_travel_time_s` ile | `missile_geometry`, `MissileTargetInfo`, `world.cpp` |
| Füze güdümlü mü (§3) | picture'a tehdit için bool, kendi füzem için koşul |
| Kaçış payı NM (§5 DEFEND), "recommit yapsam" sürümü dahil | `engagement` ya da `missile_geometry` |
| `picture.mrr_nm` (`engagement::mrr_nm` var) | `picture.hpp`, `world.cpp` |
| `weapons.r_cold_nm = r_cold_nm(enemy, own)` | `weapons.hpp`, `world.cpp` |
| Düşmanın savunma süresi `out_plan(enemy, own)` | var |
| TR, MSR | var |

### Manevralar

| Ne | Not |
|---|---|
| `Pursuit`'e gaz parametresi; `chase` = pure + art yakıcı | CHASE ve cold destek |
| Out Done sonrası gaz art yakıcı | OUT → COLD ve ABORT aynı Out'u sürdürür; `EXIT_THROTTLE` değişebilir |
| `hold` | var |

### Otopilot

| Ne |
|---|
| `OtopilotState`: Commit, Shot, Support, Trade, Defend, Out, Cold, Recommit, Chase, Abort |
| Otopilot üyeleri: savunma tipi, "vazgeçtim" bayrağı |
| `Mission`'a L&L alanı (SUPPORT sonu ABORT) |
| `decisions` yeniden: `abort`, `threat_response`, `first_shot`, `support`, `trade`, `defense`, `reassess`, `cold`, `chase` |
| Eski kalıntılar silinir: yorumdaki eski `otopilot.cpp`, `otopilot_copy.cpp`, eski K1–K5 |

## 7. Ertelenenler ve testte bakılacaklar

- Pk (ilk atış noktasının Rmax–TR arasında seçimi) — 2. aşama
- Düşmanın füzeme göre aspect'i (2. atış kararı) — 2. aşama
- BVRGym'e destek kuralı — kullanıcı ekleyecek
- Füze yavaşlamasının irtifa/hıza bağlı uyumu — gerekirse BVRGym taramasıyla
- R_cold'um < MAR ise kovalamada atış noktasına varmadan ABORT'a düşülür mü — testte
- Titreme olursa minimum faz süresi (sim zamanı otopilota)
