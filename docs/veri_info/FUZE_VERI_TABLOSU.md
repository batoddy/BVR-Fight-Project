# Füze ve sensör veri tablosu (AIM-120 / AN/APG-68)

Hazırlanma tarihi: 2026-11-12. Plan belgesi: `docs/mimari/SIMULASYON_YENILEME_PLANI.md` §5.

Kural: Her değerin kaynağı yazılıdır; kaynak bulunamazsa "bulunamadı" denir. Güven düzeyleri: resmi / açık analiz / topluluk / bulunamadı.

Erişim notu: `af.mil`, `navair.navy.mil`, `apps.dtic.mil`, `calhoun.nps.edu` ve `core.ac.uk` sayfaları bu ortamda doğrudan okunamadı (HTTP 403 / bot engeli; engeli aşmaya çalışılmadı). Bu kaynaklardan gelen değerler arama sonucu özetine dayanır ve satırda "(arama özeti)" diye işaretlidir. Doğrudan okunan kaynaklar: OPRF dosyaları, projedeki PDF'ler, arşivlenmiş Donanma bilgi dosyası, flyandwire.com yazısı.

## 1. NPS tezi ("exhaust-plume-parasitic-drag-factor ~0.6")

OPRF yorumu (`missile-code.nas` satır 388): `# Default 1. For AIM-120, Naval Postgraduate School paper suggest around 0.6. It will reduce drag during burn.` Yorum başlık, yazar, yıl vermiyor.

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| Atıf yapılan NPS tezi | **Kesin olarak bulunamadı** | - | OPRF `missile-code.nas` satır 388 yalnız "Naval Postgraduate School paper" diyor | bulunamadı | Tezin PDF'i okunamadı; doğrulama yapılamadı |
| Aday tez (doğrulanmadı) | Broadston, "A Method of Increasing the Kinematic Boundary of Air-to-Air Missiles Using an Optimal Control Approach", NPS, Eylül 2000 | - | https://calhoun.nps.edu/bitstream/10945/7735/1/methodofincreasi00broa.pdf ; https://apps.dtic.mil/sti/pdfs/ADA384590.pdf (arama özeti) | bulunamadı (yalnız aday) | AMRAAM'ı modellediği ve parazitik sürükleme katsayısını Mach'a göre verdiği söyleniyor; plume etkisi ve 0.6 değeri içinde olup olmadığı doğrulanamadı |
| Plume etkisiyle sürükleme azalması | %2-20 (Cd-on/Cd oranı yaklaşık 0.8-0.98; Şekil 5 ekseni 0.6-1.05) | oran | `docs/Dokuman/BVR dokuman/AIM120C5-Performance-Assessment-rev2.pdf` s.9, Şekil 5 ("reduction of the drag coefficient between 2-20%") | açık analiz (CFD, DCS topluluğu) | NPS değil; OPRF'in 0.6 çarpanından çok daha az azalma gösteriyor (bkz. Çelişkiler) |
| `exhaust-plume-parasitic-drag-factor` varsayılanı | 1 (AIM-120 için ~0.6 öneriliyor) | çarpan | OPRF `missile-code.nas` satır 388 | topluluk | `f16-payload.xml` AIM-120 bloğunda bu etiket yok; varsayılan 1 kullanılıyor görünüyor |
| Tezde AIM-120 sürükleme/itki/kütle değerleri | bulunamadı | - | tez okunamadı | bulunamadı | Broadston PDF'i elle indirilip bakılabilir |

## 2. OPRF `max-report-distance` birimi

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| `max-report-distance` | 65 | **metre** | `missile-code.nas` satır 2027: `"Will explode if target range is increasing and within %d meters of target."` (`me.reportDist`); satır 4458: `sprintf("... %.1f meters from: ...", range)` ile `range < me.reportDist` karşılaştırılıyor; satır 362 yorumu: "max distance from target it report it exploded ... Trig hit: Distance where it will trigger." | topluluk (kod) | Feet değil. `f16-payload.xml` satır 279 (AIM-120B) ve 110/170/230/327 diğer füzelerde de 65 |

## 3. AIM-120 resmi fiziksel veriler

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| Boy | 12 ft (3.65 m) | ft | Donanma bilgi dosyası (arşiv, 7 Ağu 2003): https://webharvest.gov/peth04/20041016160604/http:/www.chinfo.navy.mil/navpalib/factfile/missiles/wep-amr.html ("12 feet"); NAVAIR https://www.navair.navy.mil/product/AMRAAM (arama özeti) | resmi | Arşiv doğrudan okundu |
| Çap | 7 in (178 mm) | in | Aynı Donanma dosyası ("Seven inches"); NAVAIR (arama özeti) | resmi | DCS analizi s.5 Tablo 1: 180 mm |
| Atış kütlesi A/B/C/C-4 | 348 lb | lb | NAVAIR (arama özeti) | resmi | |
| Atış kütlesi C-5/6/7 | 356 lb (161.5 kg) | lb | NAVAIR (arama özeti); arşiv Donanma dosyası 2003: "356 pounds" | resmi | |
| Atış kütlesi, eski USAF formu | 335 lb (150.75 kg) | lb | af.mil bilgi formu eski sürümü (arama özeti; UNT arşiv sayfası boş döndü) | resmi (eski) | Bkz. Çelişkiler |
| Kuyruk kanat açıklığı | A/B 21 in; C/D 19 in (NAVAIR); 2003 Donanma dosyası C için 12.5 in; USAF 20.7 in | in | NAVAIR (arama özeti); arşiv Donanma dosyası; af.mil (arama özeti) | resmi (kendi aralarında çelişkili) | DCS analizi Tablo 1: 482 mm (yaklaşık 19 in) |
| Motor | Tek katı yakıtlı (boost) roket motoru, düşük dumanlı HTPB bağlayıcı | - | Donanma dosyası ("Solid Propellant Rocket"); DCS analizi s.6 | resmi / açık analiz | Üretici motor verisi yok |
| Harp başlığı | Halkalı patlayıcı-parçacıklı (ABF), 50 lb | lb | `docs/Dokuman/BVR dokuman/FIGHTER INTERCEPTS T-45 STRIKE.pdf`/`p-1290.pdf` (CNATRA P-1290, 2022) §307 | resmi | OPRF 44 lb kullanıyor |
| Toplam kütle (DCS analizi) | 157 kg | kg | `AIM120C5-Performance-Assessment-rev2.pdf` s.5 Tablo 1 | açık analiz | Kaynak [2]: Navy Training System Plan 1998; [1] daha yüksek kütle veriyor (s.5) |
| Motor bölümü kütlesi | 75.34 kg | kg | DCS analizi s.6 (kaynak [3] ATK "AMRAAM PEP Propulsion System") | açık analiz | |
| Yakıt (itici) kütlesi | 51.2559 kg (düzeltme, kaynak [7] Hazard Classification of US Military Explosives and Munitions Rev 14, 2009); CAD tahmini 51.01 kg; analizde 50 kg kullanıldı | kg | DCS analizi s.7-8 | açık analiz | Resmi belge doğrudan okunmadı |
| Yanma süresi | 7-9 s tahmin; analizde 7.75 s | s | DCS analizi s.8 | açık analiz (varsayım) | AIM-120A için 6 s boost + 5 s sustain tahmini |
| Özgül itki (Isp) | 265 (aralık 260-265) | s | DCS analizi s.6 (Sutton, Rocket Propulsion Elements, 2001'e atıfla) | açık analiz (varsayım) | |
| Toplam itki (analiz varsayımı) | 16771.9 N | N | DCS analizi s.8 | açık analiz | DCS lua önerisi s.40: 16703 N, yakıt akışı 6.43 kg/s, 8.0 s |

## 4. Sürükleme (DCS analizi, CFD)

Tüm değerler `AIM120C5-Performance-Assessment-rev2.pdf` içinden; güven: açık analiz (CFD, ±%10 belirsizlik, s.9).

| Parametre | Değer | Birim | Kaynak sayfa | Not |
|---|---|---|---|---|
| CFD hata payı | ±%10 | - | s.9 | |
| Cd (AoA 0°) aralığı | yaklaşık 0.0015-0.004, transonikte tepe | - | s.13, Şekil 11 (grafikten okuma) | "wet surface area" bazlı; irtifa 1, 5, 10, 15 km |
| Cd (AoA 0-15°, 5 km) | yaklaşık 0.002-0.027 | - | s.15, Şekil 14 (grafikten okuma) | |
| DCS lua için önerilen Cd eğrisi | Cx_k0 = 0.016 (M<<1), k1 = 0.045, k2 = 0.02, k3 = 0.016 (M>>1), k4 = 1.2; karakteristik kesit = 0.4 | - | s.40 | DCS `Cx` parametreleri; doğrudan OPRF `drag-coeff` ile karşılaştırılamaz |
| Önerilen Cd-Mach grafiği | 0-4 Mach, tepe yaklaşık 0.045 | - | s.40 (grafik) | grafikten okuma |

## 5. Menzil tabloları (irtifa x hız)

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| Düz atış, 5 km irtifa | ~35 | km | DCS analizi s.31 | açık analiz | Sabit nokta hedef |
| Senaryo 1: M0.83, 6 km, 15° tırmanış, 14 km tepe, 21° dalış, hedefte M0.9 | 60 | km | s.31 | açık analiz | Sabit nokta |
| Senaryo 4: M0.83, 6 km, 30° tırmanış/dalış | ~90 | km | s.37 | açık analiz | Sabit nokta |
| Senaryo 2: M1.5, 13 km, 22 km tepe, 6 km'de M0.9 | ~90 (kuramsal azami) | km | s.33 | açık analiz | Sabit nokta |
| Senaryo 3: M1.1, 500 m, ~11 km tepe | ~45 | km | s.35 | açık analiz | Sabit nokta |
| M1.5 atışı, M0.83'e göre | ~%15 daha fazla menzil | - | s.16 | açık analiz | |
| 30 g tutma | Mach 4'ten başlayarak 4.5 s; transonikte >10 g için 2.2 s | s | s.30 | açık analiz | |
| İrtifa etkisi | Yüksek irtifa daha uzun menzil (nitel) | - | s.30 | açık analiz | Şekil 15-18, 22-27 yalnız grafik; sayısal tablo yok |
| Mach 0.96 atış, eş hız, eş irtifa | ~40 | NM | War Thunder forumu (arama özeti): https://forum.warthunder.com/t/the-aim-120-amraam-history-design-performance-discussion/2584 | topluluk (zayıf) | Kaynak belirtilmemiş |
| Sea Harrier raporu AIM-120A zarfları | grafik var, koşul yok | - | Aynı forum başlığı (iddia: İngiliz raporu, 1980'ler ortası) | topluluk (zayıf) | Rapor görülmedi |
| OPRF `max-fire-range-nm` | 38.8 | NM | `f16-payload.xml` satır 261 | topluluk | |
| OPRF `DLZ-optimal-alt-feet` / `closing-mach` | 30000 / 2.0 | ft / Mach | `f16-payload.xml` | topluluk | |
| BMS DLZ tanımları | Ropt = %85 Raero, Rpi = %75 Raero; Rtr, Rmin hesaplı; mutlak menzil yok | - | `docs/Dokuman/BVR dokuman/aim-120_dlz_update__apg-68.pdf` s.75; `BMS-Training-Manual.pdf` §18.11.1 s.307 | topluluk (BMS belgesi) | Raero = azami kinematik menzil, hedef manevra yapmıyor |
| Rne tablosu (irtifa x hız x hedef açısı) | bulunamadı | - | - | bulunamadı | |

## 6. AN/APG-68 radarı

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| OPRF `rcsRefDistance` | 70 | NM | `Nasal/radar/apg-68.nas` satır 45 | topluluk | |
| OPRF `rcsRefValue` | 3.2 | m² | aynı dosya satır 46 | topluluk | Menzil = ref. menzil x (ref RCS / RCS)^(1/4), satır 530 |
| OPRF menzil ayarları | `maxRange` 160 NM, varsayılan 40 NM, `minRange` 5 NM; kip `rcsFactor` 0.9 / 1 / 1.15 | NM | aynı dosya satır 636-655, 1147, 1212, 1281, 1546 | topluluk | Bazı kiplerde maxRange 80 veya 40 |
| OPRF görüş bölgesi (field of regard) | ±60 azimut, ±60 yükseklik | derece | aynı dosya satır 35-37 | topluluk | |
| OPRF anlık huzme yarıçapı | 2.0 (ort.), 2.5 dikey, 1.5 yatay | derece | aynı dosya satır 42-44 | topluluk | |
| OPRF tarama genişliği (`az`) | 60 | derece | aynı dosya satır 639 | topluluk | |
| Azimut taraması | ±60° tam tarama (ayrıca ±30, ±25, ±10 seçenekleri) | derece | DCS F-16 APG-68(V5) tartışması (arama özeti): https://forum.dcs.world/topic/303895-agp-68v5-radar-azimuth-and-elevation-bar-issue | topluluk | Resmi belge bulunamadı |
| Azami algılama menzili (genel) | ~50 mil (80 km) | mil | Wikipedia türevi sayfalar (arama özeti) | topluluk (zayıf) | Hedef RCS belirtilmemiş |
| Blk 50 radar menzil artışı | %30 | - | https://www.globalsecurity.org/military/systems/aircraft/systems/an-apg-68.htm (arama özeti) | topluluk (zayıf) | Mutlak menzil yok |
| DCS F-16 oyun ölçümü | 40 NM maksimum izleme, hedef RCS 4 m² | NM | https://flyandwire.com/2022/09/22/f-16-an-apg-68-max-track-range-study/ (doğrudan okundu) | topluluk (oyun ölçümü) | Gerçek spesifikasyon değil; yazı DCS yamasından sonra eskimiş |
| "V9: 55 NM, 5 m² hedef" iddiası | 55 | NM | DCS forum (arama özeti), ikincil | topluluk (zayıf) | Doğrulanmadı |
| Resmi APG-68 menzili (hedef RCS ile) | bulunamadı | - | - | bulunamadı | |

## 7. BMS AIM-120 `FighterPitbull`, `HighAspectFighterHusky`

| Parametre | Değer | Birim | Kaynak | Güven | Not |
|---|---|---|---|---|---|
| Değişken anlamları | `AspectSelectorSize` (HPRF için hedef açısı 180°'ye bu kadar derece yakın olmalı), `HuskyMinClosure` (HPRF için asgari kapanma hızı), `HighAspectBomberHusky` / `HighAspectFighterHusky` (HPRF menzil kapısı; LARGE / MED-UNKN hedef), `BomberPitbull` / `FighterPitbull` (MPRF menzil kapısı) | - | `docs/Dokuman/BVR dokuman/aim-120_dlz_update__apg-68.pdf` s.77 (doğrudan okundu) | topluluk (BMS sürüm notu) | SMALL seçilirse MPRF menzili MED'e göre yaklaşık 1/3 azalır |
| Sayısal değerler | bulunamadı | - | Belge: "Ranges coded are rough guesses for the AIM-120B" | bulunamadı | Sayılar BMS `.dat` dosyasında; kamuya açık yerde değer bulunamadı. Forum başlığı değişkenleri anıyor, sayı yok: https://forum.falcon-bms.com/post/2698 (arama özeti) |
| OPRF pitbull | 10 NM (füze-hedef) | NM | `libraries/station-manager.nas` satır 154-162 | topluluk | |

## Bulunamayanlar

- OPRF yorumunun atıf yaptığı NPS tezi (başlık/yazar/yıl): doğrulanamadı; yalnız aday Broadston (2000). Tezin AIM-120 değerleri okunamadı.
- Rne / azami menzilin irtifa x hız x hedef açısı sayısal tablosu (açık kaynakta).
- AIM-120 motor itki-zaman eğrisi (resmi) ve resmi sürükleme eğrisi.
- APG-68 için resmi tespit menzili (hedef RCS ile) ve resmi gimbal açısı.
- BMS `FighterPitbull` / `HighAspectFighterHusky` sayısal değerleri.
- af.mil, NAVAIR, DTIC, Calhoun sayfalarının doğrudan okunması (erişim engeli; arama özeti kullanıldı).

## Çelişkiler

1. Atış kütlesi: 291 lb (OPRF), 335 lb (USAF eski), 348 lb (NAVAIR A/B/C/C-4), 356 lb (NAVAIR/Donanma C-5/6/7), 157 kg = 346 lb (DCS analizi). OPRF'in 291 lb değeri resmi verinin belirgin altında.
2. Harp başlığı: 50 lb (CNATRA P-1290) ile 44 lb (OPRF).
3. Plume sürükleme azalması: OPRF 0.6 çarpanı (yaklaşık %40 azalma, NPS atfıyla) ile DCS analizi %2-20 uyuşmuyor.
4. İtici kütlesi: OPRF 130 lb (59 kg) ile DCS analizi 51.3 kg (113 lb). Motor: OPRF çift kademeli (2700 lbf x 10 s + 280 lbf x 18 s), DCS analizi tek kademeli 7-9 s, yaklaşık 16.7 kN (3770 lbf).
5. Kanat açıklığı: 21 / 19 / 20.7 / 12.5 in, varyant ve kaynağa göre değişiyor.

Karar gerektiren soru: İtki/kütle başlangıç değeri OPRF'ten mi (topluluk) yoksa DCS analizinden mi (7.75 s, ~16.7 kN, 157 kg; açık analiz) alınsın? Tabloya ikisi de yazıldı, seçim yapılmadı.
