# Uçuş kontrolü tasarımı (NASA AML)

Kaynaklar: NASA CR-4160 (1988) §3.2.2, §4.2–4.4; NASA CR-145004 (1976) bang-bang yatış kontrolü. Plan: `docs/mimari/SIMULASYON_YENILEME_PLANI.md` §3.2.

**GEÇİCİ DEĞERLER:** Bu belgede ve kodda "geçici" diye işaretli kazanç ve eşiklerin F-16 için kaynaklı değeri yok. F-4 raporlarından ve modelden ölçülerek seçildi (kullanıcı onayı 2026-11-12). Uçuş testlerine göre güncellenebilir.

## Girdi ve çıktı

- **Girdi:** `FlightCommand` (yön °, irtifa m, gaz 0..1, g_limit). Her fizik adımında (1/120 s) çağrılır.
- **Çıktı:** `model.set_controls(aileron, elevator, rudder, throttle)`.

## Katmanlar

```
1. istenen yön vektörü D (NED)      yön komutu + irtifa hatasından istenen uçuş yolu açısı
2. hata açısı ε = ∠(V̂, D)            V̂: hız vektörü yönü (uçuş yolu; burun değil)
   ε > ε_geçiş → 3a manevra düzlemi    (AML §3.2.2, §4.2)
   ε ≤ ε_geçiş → 3b işaret etme        (AML §4.4)
3a. Manevra düzlemi: V̂ ile D'nin düzlemi. İstenen dik ivme a = n·g·ê (ê: düzlemde V̂'ye dik, D'ye doğru).
    Kaldırma vektörü L = a − g⊥ (g⊥: yerçekiminin V̂'ye dik bileşeni). İstenen yatış φ_d = L'nin V̂ etrafındaki açısı;
    istenen yük n_d = |L|/g, g_limit ile kırpılır. n: g_limit (AML: büyük hatada azami yük).
3b. İşaret etme: D gövde eksenlerinde çözülür → yatış hatası (D'yi gövde x-z düzlemine getiren) ve yunuslama hatası.
    φ_d = φ + Kφ_p·yatış hatası; n_d = cosγ·cosφ'ye göre düz uçuş yükü + V·Kθ·yunuslama hatası / g.
4. Yatış hızı komutu (CR-145004 bang-bang + doğrusal bölge):
    p_kom = sign(φe) · min(p_max, sqrt(2·ṗ_max·|φe|), Kφ·|φe|)      φe = φ_d − φ (−180..180)
5. Yük komutu: n_kom = n_d (sert dönüşte a_n/V düzeltmesi n üzerinden zaten dahil, AML §4.3).
6. FBW adaptörü (BVRGym JSBSim F-16 f16.xml):
    aileron = p_kom / π  (fcs/roll-rate-norm = p·0.31821 → 1.0 ≈ 180°/s; f16.xml satır ~367)
    elevator: ölçülen yük (forces/load-factor) ile n_kom farkından PI (FBW yunuslama kanalı g ve q karışımı; temiz g komutu değil)
    rudder = 0 (FBW yaw kanalı yanal yükü sıfırlar)
    throttle = komut
```

## İrtifa

İstenen uçuş yolu açısı: γ_d = atan(clip(K_h·Δh, −Vz_aşağı, +Vz_yukarı) / V). Başlangıç değerleri eski sim'den, uçuşta denenmiş: K_h = 0.08 1/s, Vz_yukarı = 60 m/s, Vz_aşağı = 120 m/s (`src/sim/flight_control.py`). **Geçici.**

## Uygulama ve tasarımdan sapmalar (2026-11-12)

**Kullanıcı onayı (2026-11-12):**
- Sapma 1 (işaret etme hız vektörüyle) ve sapma 2 (geçiş eşiği hesaplanıyor) onaylandı.
- Out dönüşünde irtifa korunur. Ölçüm: irtifa kaybına izin vermek dönüşü hızlandırmıyor (10 km'de 23.8 → 25.0 s, −1065 m), yalnız hıza çeviriyor (+0.09 M). "Dalarak hız kazanma" taktik karar olarak C++'a bırakıldı.

Kod: `aml.py` (katman 1–5), `fbw.py` (6), `geometry.py` (NED/gövde/yatış açısı), `parameters.py`. Ölçüm: `src/simulator/tools/flight_tests.py`.

1. **3b işaret etme hız vektörüyle, yerçekimi dengeli kuruldu.** Yazıldığı haliyle (D gövdede, φ_d = φ + Kφ_p·yatış hatası) küçük hatada tekil: 5° yanal hatada yatış hatası ≈ 90° çıkar, D burna yaklaştıkça açı belirsizleşir; burun hız vektörünün α kadar üstünde olduğundan düz uçuşta D "aşağıda" görünür (yatış hatası 180°). Yerine: istenen dik ivme a = V·K_POINT·ε·ê (hız vektörü D'ye ε ile orantılı hızda döner), L = a − g⊥, φ_d ve n_d L'den. Kφ_p gerekmez; Kθ yerine tek kazanç K_POINT.
2. **ε_geçiş sabit değil.** İşaret etme kaldırması |L| g_limit'i aştığı anda manevra düzlemine geçilir; iki kip bu noktada sürekli. 10 km M0.9 out'ta (ulaşılabilir ~4 g) ≈ 16°, 3 g crank'te ≈ 12°.
3. **Yerçekimi dengeleme, `GRAVITY_COMPENSATION` (karar 2026-11-12: 1.0).** AML orijinali (0) denendi: 10 km out −1360 m. C++ out komutuyla (irtifa −3300 ft, 6 g, tam gaz) tarama, 10 km:

   | Değer | En alçak (hedef −1006 m) | Mach +20 s |
   |---|---|---|
   | orijinal | −2326 m | 1.12 |
   | 0.85 | −1312 m | 1.15 |
   | 1.0 | −1043 m | 1.16 |

   Dönüş süresi hepsinde ~25 s. 1.0'da alçalma miktarını C++ irtifa komutu belirler. **Manevra düzleminde dikey bileşen korunur.** L'nin yukarı bileşeni (a_yukarı + g·cosγ) tutulur, kalan yük (|L| = g_limit) yanala verilir. Düzlem (V̂, a) düzlemi. AML'deki (V̂, D) düzleminde 180°'lik dönüşte irtifa hatası düzlemi yalnız ~1° eğiyordu: 10 km out'ta −505 m.
4. **Yön farkı ±90° ile kırpılır** (D iz açısının en fazla 90° önünde). Yoksa 180°'de D ≈ −V̂ olur ve düzlemi irtifa gürültüsü seçer (takla / split-S).
5. **İşaret etmede dikey ivme sınırı** A_UP = 1 g, A_DOWN = 0.5 g (eski sim). Yoksa 250 m'den büyük her alçalma ters dönüp çekmeye gider.
6. **Ulaşılamayan yük.** JSBSim F-16 FBW 10 km M0.9'da tam çubukta ~3.95 g veriyor (q geri beslemesi + hücum açısı çizelgesi). 6.5 g'ye göre kurulan 81° yatışla uçak alçalır. Elevator 1 s'den uzun tam çekişteyse düzlem ölçülen yükle kurulur, yük komutu g_limit'te kalır (eski sim SATURATED_S yaklaşımı; AML §4.3'teki "komut yüküne ulaşamama" sorunu).
7. **ṗ_max sabit değil, q̄ ile ölçeklenir.** Tam aileron basamağında ṗ_max/q̄ 2–10 km, M0.5–1.1'de 16.1–16.6 (°/s²)/kPa: neredeyse sabit.
9. **Yatış girişinde yük sınırı (2026-11-12).** Yatış oturmadan çekilen yük out başında tırmanma yapıyordu (5 km +84 m, 6.69 g). Kural: n·cos(φ_şimdi) ≤ n_d·cos(φ_d). Ayar sayısı yok. 5 km: +3 m, 6.51 g.
8. **CR-145004 "pitch slowly"** (yatış hatası > 30° ve p > 50°/s iken yük hedefi ortalama) eklendi; ölçümde etkisi görülmedi (out_5km aynı).

## Ölçümler (JSBSim F-16, BVRGym kopyası)

| Büyüklük | 10 km M0.9 | 5 km M0.9 | Yöntem |
|---|---|---|---|
| p_max (tam aileron, 4 s) | 160–170°/s (sola 134) | 180–182°/s (sola 143) | `aileron-cmd-norm` = ±1 basamağı |
| ṗ_max | 251–255°/s² | 512–518°/s² | aynı, 6 adımlık türev; q̄ = 15.2 / 31.0 kPa |
| Yatış hızı döngüsü τ | 0.18 s | 0.08 s | aileron 0.1 basamağı, %63 |
| 6.5 g basamağında ulaşılan yük (düz kanat) | 3.95 g (elevator doygun) | 6.45 g (0.5 s'de) | g PI ile 6.5 g basamağı |

## Parametreler (hepsi `flight_control/parameters.py`'de, kaynak notuyla)

| Ad | Anlam | Son değer | Kaynak |
|---|---|---|---|
| ε_geçiş | Manevra düzlemi / işaret etme sınırı | türetilir: V·K_POINT·ε'nin g_limit'e ulaştığı açı | sapma 2 |
| p_max | Azami yatış hızı komutu | 180°/s (aileron 1.0) | f16.xml roll-rate-norm |
| ṗ_max | Azami yatış ivmesi | 16.1 (°/s²)/kPa · q̄ | ölçüm |
| Kφ | Doğrusal bölge kazancı | 2.8 1/s | geçici: τ=0.18 s ile ζ≈0.7; 2.0 ve 4.0 de denendi, yatış izleme farkı < 1° |
| K_POINT (Kφ_p, Kθ yerine) | İşaret etme: hız vektörünün D'ye dönüş hızı / ε | 0.5 1/s | geçici: eski sim K_VZ = 1/TAU_HEAD = 0.5; K_h ile ζ = 1.25 |
| A_UP, A_DOWN | İşaret etmede dikey ivme sınırı | 1 g, 0.5 g | eski sim A_UP_MAX, N_MIN; geçici |
| g PI | Elevator g döngüsü | KP 0.2, KI 0.6 | geçici: 1→2/4/6.5 g basamağında aşmasız (eski sim KP 0.08: +0.19 g aşma) |
| SATURATED_S | Ulaşılamayan yük süresi | 1 s | eski sim, geçici |
| pitch slowly | Yatış sürerken yük yarıya | 30°, 50°/s | CR-145004 s.60 (F-4), geçici |
| K_h, Vz | İrtifa | 0.08 1/s, +60 / −120 m/s | eski sim, geçici |
| N_MIN | En küçük yük komutu | 0.5 g | eski sim |
| varsayılan g_limit | Komutta yoksa | 6 g | eski sim DEFAULT_TURN_G |
