# Simülatör kaynakları (fonksiyon fonksiyon)

Hazırlanma: 2026-11-12. Satır numaraları, dosyaların o tarihte indirilen sürümüne göredir; GitHub'da `master` değişirse kayabilir. Fonksiyon adı ile aramak daha güvenlidir.

Güven etiketleri:
- **kaynak:** doğrudan alındı;
- **uyarlama:** kaynaktaki yöntem değiştirilerek kullanıldı (neden yazılı);
- **ölçüm:** bizim modelde ölçülerek bulundu;
- **eski sim:** `src/sim/flight_control.py`'de daha önce uçuşta denenmiş değer;
- **geçici:** kaynağı yok, ayar.

## Ana kaynaklar ve nasıl kullanıldıkları

Bütün kod Python'dur (`src/simulator/`). C++ otopilota (`src/otopilot/`) bu çalışmada dokunulmadı. Hiçbir kaynaktan satır satır kod kopyalanmadı. İstisnalar: JSBSim XML model dosyaları ve eski sim'in `connector.py`'si (aşağıda belirtildi).

| Kısaltma | Kaynak | Bağlantı | Nasıl kullanıldı |
|---|---|---|---|
| CR-4160 | Burgin & Sidor, *Rule-Based Air Combat Simulation*, NASA CR-4160, 1988 (AML) | https://ntrs.nasa.gov/citations/19890018022 · PDF: https://ntrs.nasa.gov/api/citations/19890018022/downloads/19890018022.pdf | **Doküman okunarak tasarım koda döküldü.** PDF metne çevrildi (pdftotext; taranmış, OCR'lı). §3.2.2, §4.2–4.4 okundu. Yöntem `flight_control/TASARIM.md`'ye tasarım olarak yazıldı, sonra Python'da sıfırdan kodlandı. Raporun ekindeki Fortran listeleri çevrilmedi. Sapmalar TASARIM.md'de. |
| CR-145004 | Burgin & Eggleston, *Design of an All-Attitude Flight Control System to Execute Commanded Bank Angles and Angles of Attack*, NASA CR-145004, 1976 | https://ntrs.nasa.gov/citations/19760020159 · PDF: https://ntrs.nasa.gov/api/citations/19760020159/downloads/19760020159.pdf | **Doküman okunarak yöntem ve eşikler alındı.** Bang-bang yatış kontrolünün geçiş eğrisi fikri (√(2·ṗ_max·\|φe\|)) ve "pitch slowly" eşikleri (30°, 50°/s) metinden alındı. F-4'e özgü kazanç listeleri ve Fortran kodu (LACNTR, LOCNTR) kullanılmadı. |
| CR-2582 | Burgin, Fogel, Phelps, *An Adaptive Maneuvering Logic Computer Program…*, Vol. I, NASA CR-2582, 1975 | https://ntrs.nasa.gov/citations/19750022744 | **Yalnız bağlam için.** Arama sonucunda tanıtım bilgisi okundu; koda girmedi. |
| OPRF füze | FlightGear OPRF `missile-code.nas` (Nasal dili, GPL-2.0+) | https://github.com/NikolaiVChr/OpRedFlag/blob/master/emesary-damage-system/nasal/missile-code.nas | **Nasal kaynak kodu GitHub'dan indirilip okundu; formüller Python'a çevrildi.** `drag()` Cd0(M) formülü ve `energyBleed()` tablo değerleri birebir aynı sayılarla, sade Python fonksiyonlarına yeniden yazıldı (`zero_lift_drag_coefficient`, `turn_bleed`). `speedChange()` / `limitG()` yapısı örnek alındı. `guide()`, `checkForGuidance()`, `proximity_detection()` çevrilmedi; boş fonksiyonlarda yalnız atıf olarak geçiyor. |
| OPRF istasyon | OPRF `station-manager.nas` | https://github.com/NikolaiVChr/OpRedFlag/blob/master/libraries/station-manager.nas | **Kod okunup değer alındı.** AIM-120 uçuş içi fonksiyonundaki 10 NM eşiği `aim120c5.yaml`'a parametre olarak yazıldı. Kod çevrilmedi. |
| OPRF F-16 | OPRF F-16 paketi: `Systems/f16-payload.xml`, `Nasal/radar/apg-68.nas`, `Systems/jsb-autoflight.xml`, `Systems/jsb-controls.xml` | https://github.com/NikolaiVChr/f16 | `f16-payload.xml`: **XML okunup parametre değerleri yaml'a kopyalandı** (`aim120b_oprf.yaml` tamamen; `aim120c5.yaml`'da sürükleme, G, güdüm alt sınırı, kurulma, imha, arayıcı alanları). `apg-68.nas`: ±60° görüş alanı değeri alındı (alt ajan okudu). `jsb-autoflight.xml`, `jsb-controls.xml`: **okunup yapı incelendi, koda alınmadı** (karşılaştırma ve plan belgesi için). |
| OPRF radar | `radar/radar-system.nas`, `radar/rcs_mand.nas` | https://github.com/NikolaiVChr/OpRedFlag/tree/master/radar | **Okundu, henüz kullanılmadı.** Notch kuralı (30 kt) ve RCS tablosu Faz 5 (sensörler) için plana yazıldı. |
| JSBSim F-16 | JSBSim F-16 modeli (Erik Hofman, GPL); BVRGym'deki kopya | https://github.com/JSBSim-Team/jsbsim/tree/master/aircraft/f16 | **Dosyalar olduğu gibi kopyalandı** (`jsbsim_data/`: `f16.xml`, `reset00.xml`, `Systems/`, motor `F100-PW-229.xml`, `direct.xml`); değiştirilmedi. FCS kanalları okunarak FBW adaptörünün ölçeği (aileron = p/π) çıkarıldı. |
| BVRGym | Scukins vd., *BVR Gym* (Python + JSBSim) | https://arxiv.org/abs/2403.17533 · yerel: `BVRGym/` | **Kaynak kodu okunup davranış yeniden yazıldı.** JSBSim başlatma (`FDMObject.reset`), property adları, iniş takımı, dt ve tick süresi `jsbsim_model.py`'de sıfırdan yazıldı. BVRGym paketi içe aktarılmıyor, Python kodu kopyalanmadı. |
| Eski sim | `src/sim/` (bu projenin önceki Python sim'i) | yerel | `connector.py` **aynen kopyalandı** (TCP protokolü aynı kalsın diye). `simulation.py`, `recorder.py`, `main.py`, `scenario.py` **uyarlanarak yeniden yazıldı** (mesaj ve kayıt biçimi korundu). `flight_control.py`'deki sabitler (K_H, VZ, N_MIN, A_UP, SATURATED_S, varsayılan g) **değer olarak taşındı**. |
| Analiz | *AIM-120C-5 Performance Assessment* rev2 (DCS topluluğu, CFD tabanlı açık analiz) | yerel: `docs/Dokuman/BVR dokuman/AIM120C5-Performance-Assessment-rev2.pdf` | **PDF okunup değerler alındı** (alt ajan, pdftotext). Kütle, itici, itki, yanma süresi `aim120c5.yaml`'a yazıldı. Menzil sonuçları kalibrasyon için bekliyor. |
| Veri tablosu | Bütün füze/radar değerleri, kaynakları, çelişkiler | yerel: `docs/veri_info/FUZE_VERI_TABLOSU.md` | **Alt ajanın web araştırmasıyla derlendi** (resmi bilgi formları, OPRF, analiz PDF'i). Bazı resmi sayfalar 403 verdiği için arama özetine dayanan satırlar orada işaretli. |
| USSA 1976 | *U.S. Standard Atmosphere, 1976*, NOAA/NASA/USAF (NASA-TM-X-74335) | https://ntrs.nasa.gov/citations/19770009539 | **Standart formüller ve tablo değerleriyle kodlandı.** Katman tablosu ve denklemler standardın bilinen değerlerinden yazıldı; bu çalışmada PDF açılmadı. Deniz seviyesi ve 11 km değerleri testte standardın tablo değerlerine karşı doğrulanıyor. |
| WGS84 | NIMA TR8350.2, *Department of Defense World Geodetic System 1984* | NGA yayını (belge adıyla aranabilir) | **Standart sabitlerle kodlandı** (a, f, GM). Dönüşüm formülleri standart jeodezi formülleri; belge bu çalışmada açılmadı. Gidiş-dönüş testiyle doğrulanıyor. |
| Ölçüm | `tools/flight_tests.py` ve scratchpad deney betikleri | yerel | **Kaynağı olmayan değerler ölçümle bulundu:** ṗ_max/q̄, yatış hızı zaman sabiti, g PI kazançları, `GRAVITY_COMPENSATION` taraması. Sonuçlar TASARIM.md'de. |

## Uçuş kontrolü (`flight_control/`)

### `aml.py`

| Fonksiyon | Ne yapar | Kaynak | Etiket |
|---|---|---|---|
| `AMLController.update` | Katman sırası: yön vektörü → kaldırma → yatış hızı → yük → FBW | CR-4160 §4.2 (komut üçlüsü: düzlem, yük, gaz → tutum → gövde hızları) | uyarlama |
| `desired_gamma` | İrtifa hatasından istenen uçuş yolu açısı | Eski sim irtifa döngüsü (`K_H`, `VZ_*`) | eski sim |
| `desired_direction` | İstenen yön vektörü D; yön farkı ±90° ile kırpılır | CR-4160 §3.2.2 (D'ye yönelme); ±90° kırpma bizim (180°'de düzlem belirsiz kalıyordu) | uyarlama |
| `lift_command`, işaret etme kolu | Küçük hata: hız vektörü D'ye ε ile orantılı döner; yerçekimi dengelenir | CR-4160 §4.4 (pointing control). Gövde ekseni yerine hız vektörüyle yeniden kuruldu: küçük hatada tekillik (TASARIM.md sapma 1, kullanıcı onaylı) | uyarlama |
| `lift_command`, manevra düzlemi kolu | Büyük hata: g_limit'te dönüş, `GRAVITY_COMPENSATION` kadar yerçekimi dengelenir | CR-4160 §3.2.2 (manevra düzlemi), §4.2; dengeleme oranı bizim (k taraması, TASARIM.md sapma 3) | uyarlama |
| `lift_command`, kip geçişi | İşaret etme yükü g_limit'i aşınca manevra düzlemi | CR-4160'ta sabit eşik var; bizde süreklilik koşulu (TASARIM.md sapma 2, onaylı) | uyarlama |
| `roll_rate_command` | p = sign·min(p_max, √(2·ṗ_max·\|φe\|), Kφ·\|φe\|) | CR-145004 bang-bang aileron kontrolü ve geçiş eğrisi (içindekiler s.64; metin ~s.62–68); doğrusal bölge aynı raporun "linear lateral control"ı (s.62) | kaynak (yöntem) + ölçüm (ṗ_max) |
| `roll_in_limit` | Yatış oturmadan çekilen yükün dikey bileşenini sınırlar: n·cosφ_şimdi ≤ n_d·cosφ_d | Uçuş mekaniği bağıntısı (koordineli dönüşte düşey yük = n·cosφ); fikir eski sim `ENTRY_G` | uyarlama |
| `pitch_slowly` | Büyük yatış sürerken yük hedefi şimdiki ile istenenin ortası | CR-145004 ~s.59–60, LOCNTR "pitch slowly" modu (yatış hatası >30°, yatış hızı >50°/s). Raporda hücum açısının ortası; bizde yük faktörünün | uyarlama |

### `fbw.py`

| Fonksiyon | Ne yapar | Kaynak | Etiket |
|---|---|---|---|
| `FBWAdapter.update` | Yatış hızı → `aileron-cmd-norm` (×1/π); yük → elevator PI | JSBSim F-16 `f16.xml` Roll kanalı (~364–440: `roll-rate-norm` = p·0.31821) ve Pitch kanalı (~503–680: g/q karışımı, temiz g komutu yok → PI). PI kazançları ölçüm/geçici | kaynak + geçici |
| `FBWAdapter.load_unreachable` | Elevator 1 s'den uzun tam çekişteyse yük ulaşılamaz | Eski sim `SATURATED_S`; CR-4160 §4.3 ("commanded load factor" ulaşılamama sorunu) | eski sim |

### `geometry.py`

| Fonksiyon | Ne yapar | Kaynak | Etiket |
|---|---|---|---|
| `wrap_pi`, `unit`, `perpendicular` | Açı sarma, birim vektör, dik bileşen | Genel vektör cebiri | — |
| `direction` | Yön + uçuş yolu açısından NED birim vektör | Genel (küresel → kartezyen) | — |
| `body_axes` | Euler açılarından gövde eksenleri | Standart 3-2-1 (yaw-pitch-roll) dönüşüm matrisi; JSBSim'in de kullandığı | — |
| `level_frame`, `bank_angle` | Hız vektörü etrafında yatışsız eksen ve kaldırmanın yatış açısı | CR-4160 §3.2.2 manevra düzlemi koordinat sistemi (Şekil 3-1) | uyarlama |

### `parameters.py`

| Sabit | Değer | Kaynak | Etiket |
|---|---|---|---|
| `G0_MPS2` | 9.80665 | ISO 80000-3 standart yerçekimi | kaynak |
| `DEFAULT_G_LIMIT`, `N_MIN`, `K_H`, `VZ_UP_MAX`, `VZ_DOWN_MAX`, `A_UP_MAX`, `SATURATED_S` | 6, 0.5, 0.08, 60, 120, 1 g, 1 s | `src/sim/flight_control.py` | eski sim, geçici |
| `A_DOWN_MAX` | 0.5 g | `N_MIN`'den türetildi | eski sim, geçici |
| `HEADING_LEAD_MAX_DEG` | 90° | Geometri: 180°'de D ≈ −V̂ olmasın | uyarlama |
| `K_POINT` | 0.5 1/s | Eski sim `K_VZ` ve `1/TAU_HEAD` | geçici (F-4 → F-16 onaylı) |
| `P_MAX_DPS` | 180°/s | `f16.xml` `roll-rate-norm` | kaynak |
| `PDOT_PER_QBAR` | 16.1 (°/s²)/kPa | Tam aileron basamağı, 2/5/10 km, M0.5–1.1 | ölçüm |
| `K_PHI` | 2.8 1/s | τ = 0.18 s ölçümü ile sönüm ≈ 0.7 | geçici |
| `PITCH_SLOW_ROLL_ERROR_DEG`, `PITCH_SLOW_ROLL_RATE_DPS` | 30°, 50°/s | CR-145004 ~s.59–60 (F-4 değerleri) | kaynak (F-4), geçici |
| `AILERON_PER_RAD_S` | 1/π | `f16.xml` `roll-rate-norm` 0.31821 | kaynak |
| `KP_G`, `KI_G` | 0.2, 0.6 | g basamak testleri (eski sim 0.08 / 0.6'dan başlandı) | ölçüm, geçici |
| `GRAVITY_COMPENSATION` | 1.0 | k taraması, kullanıcı kararı | ölçüm |

## Uçak (`physics/jsbsim_model.py`, `aircraft.py`)

| Fonksiyon | Kaynak |
|---|---|
| `JSBSimModel.__init__`: model `f16`, dt 1/120 s | JSBSim F-16 (yukarıda); dt: BVRGym `scripts/f16_test.xml` |
| `JSBSimModel.reset`: ic property'leri, motor çalışır, iniş takımı yukarı | BVRGym `jsb_gym/simObjects/FDMObject.py` `reset()`, `set_retract_gear()` |
| `set_controls`, okuyucular: property adları | BVRGym `FDMObject.py` getter/setter'ları; JSBSim property kataloğu |
| `qbar_pa`: 1 lbf/ft² = 47.880259 Pa | Birim dönüşümü (tanım) |
| `Aircraft.health = 1`, isabette −1 | BVRGym `healthPoints` davranışı (eski sim) |
| Tick = 10 fizik adımı | BVRGym `f16_config.py` `Control_time_step=10` |

## Fizik (`physics/`)

| Fonksiyon | Kaynak |
|---|---|
| `atmosphere.state`: katman tablosu, jeopotansiyel irtifa, R = 287.05287, γ = 1.4 | USSA 1976 (Tablo 4 katmanları, denklemler 23 / 33a / 33b) |
| `earth.geodetic_to_ecef`, `ecef_to_geodetic`, `ned_axes` | WGS84 (TR8350.2) sabitleri; dönüşüm standart jeodezi formülleri (Bowring başlangıçlı yineleme) |
| `earth.gravity_ecef`: −GM·r/\|r\|³ | Nokta kütle yerçekimi, WGS84 GM |

## Füze (`weapons/`, `data/missiles/`)

| Fonksiyon / alan | Kaynak | Etiket |
|---|---|---|
| `zero_lift_drag_coefficient` | OPRF `missile-code.nas` `drag()` (~2924): parçalı Cd0(M) × 5 × `drag-coeff` | kaynak |
| `turn_bleed` | OPRF `energyBleed()` (~3050) ve `bleed32800at0g/25g`, `bleed0at0g/25g` tabloları: g ve irtifada doğrusal ara değer | kaynak |
| `Missile.step`: itki − sürükleme − dönüş kaybı + güdüm + yerçekimi | OPRF `speedChange()` (~3037) yapısı; ECEF ve WGS84 yerçekimi bizim (düz dünya yerine) | uyarlama |
| `Missile.step`: G sınırı, hız yönündeki bileşenin atılması | OPRF `limitG()` (G sınırı); dik bileşen genel fizik | uyarlama |
| `Missile.step`: itici, verilen itkiyle orantılı tükenir | İtki/yakıt ilişkisi (ṁ = T / (Isp·g0) ile aynı, Isp ayrıca gerekmez) | — |
| `Missile._check_end`: kendini imha, yere çarpma, yanma sonrası Mach < güdüm alt sınırı → kayıp | OPRF `self-destruct-time-sec`, `min-speed-for-guiding-mach` (`f16-payload.xml` `<aim-120>`) | kaynak |
| `Seeker.update` (boş) | OPRF `guide()` "sample" (~3220), `checkForGuidance()` (~3440), `station-manager.nas` AIM-120 (~159) | kullanıcı yazacak |
| `guidance.acceleration_command` (boş) | OPRF `guide()` PN, `steering_speed_G()` | kullanıcı yazacak |
| `fuze.detonates` (boş) | OPRF `proximity_detection()` (~4220), satır 2027 | kullanıcı yazacak |
| `aim120c5.yaml`, `aim120b_oprf.yaml` | Her satırın yanında; özet `docs/veri_info/FUZE_VERI_TABLOSU.md` | — |

## İncelenip kullanılmayanlar

| Kaynak | Neden |
|---|---|
| OPRF F-16 otopilotu `jsb-autoflight.xml` | Seyir otopilotu (yatış ≤30°); yapı fikir verdi, kod alınmadı |
| OPRF F-16 JSBSim modeli `f16b50.xml` | FlightGear bağımlılığı (~180 property); geçiş kullanıcı kararına bırakıldı |
| ArduPilot TECS | Hız komutu gelince (sonraki aşama) otomatik gaz için referans: https://ardupilot.org/plane/docs/tecs-total-energy-control-system-for-speed-height-tuning-guide.html |
| DARPA AlphaDogfight | Yalnız yaklaşımı doğruladı (FBW üstünde pilot katmanı): https://www.jhuapl.edu/technical-digest/issues/vol-36-no-2-2022/alphadogfight-trials-bringing-autonomy-air-combat |
| CMO | Yalnız menzil kıyası için (telif); koda girmedi |
