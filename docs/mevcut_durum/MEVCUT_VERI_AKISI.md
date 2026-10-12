# Mevcut sistemin veri akışı ve bulgular

**10 Ekim 2026.** Bu belge bugün çalışan sistemi anlatır; hedef mimari [BVR_SISTEM_MIMARISI.md](../mimari/BVR_SISTEM_MIMARISI.md) ve [CORE_MIMARI.md](../../../CORE_MIMARI.md) belgelerindedir. Aşağıdaki sayılar `scenario.yaml` ile kısa bir uçuş koşulup çıktıları okunarak ölçüldü; tahmin değildir. Görsel sürüm: [MEVCUT_VERI_AKISI.html](MEVCUT_VERI_AKISI.html).

## 0. Düzeltme durumu

**Düzeltildi (10 Ekim 2026).** Kayıt BVR alanlarına indirildi: uçak 19, füze 17 sütun; takma ad tekrarları kalktı. `Latitude` tekrar `position/lat-gc-deg`. Uygulanan dış komut (`command/heading-deg`, `command/altitude-m`, `command/throttle-norm`) snapshot başına kaydediliyor. `events.jsonl` ve `manifest.json` eklendi, `state/` klasörü ve `KAYIT_ICERIGI.txt` kalktı. Arayüzde ölçülen hız `velocities/vt-fps`'ten okunuyor, mühimmat sahipliği dosya adındaki ajan anahtarından çözülüyor, manifest koordinat referansı doğrulanıyor. Kayıt adına basınca oturum paneli manifestin bildirdiklerini gösteriyor; olay işaretleri ve mühimmat sonucu Görünüm menüsündeki **Füze vuruş bilgisi** kutusuna bağlı ve varsayılan kapalı. Belgeler gerçeğe hizalandı.

**Sonraki değişiklik (12 Ekim 2026).** Bu çözüm aşıldı: kayıt Tacview takma adlarını bıraktı, boşluksuz ve birimi adında taşıyan kendi sütun adlarını yazıyor; enlem doğrudan WGS84 jeodezik (`lat_deg` ← `position/lat-geod-deg`), BVRGym'in iç jeosantrik enlemi `lat_gc_deg` sütununda. `fcs/throttle-cmd-norm` kalktı (uçakta komut değerinin birebir kopyası, füzede sabit). Uçak 18, füze 16 sütun. Görüntüleyici iki adlandırmayı da okuyor ve ECEF'i tek referanstan kuruyor. Geçerli sözleşme: `BVRGym-Goruntuleyici/VERI_AKISI.md`.

**Açık kalanlar.** BVRGym'in iç işleyişine dokunulmaması kararı gereği füze fırlatma hızı (4.2) ve `is_target_lost()` (4.3) olduğu gibi bırakıldı. ACMI yazıcı/okuyucu (5.5) ve `fdm.run()` dönüş değerinin kontrolü (5.6) henüz yapılmadı. Planlanan rota, geometri ve zarf kayıtları kapsam dışı; ayrı dosyalar olarak sonra eklenecek. Komut sütunlarını arayüz okuyor ama henüz göstermiyor; hedef atama çizgisi de eklenmedi.

Aşağıdaki bölümler analizin kendisidir; 4. ve 5. bölümdeki maddeler bulgu anındaki durumu anlatır.

## 1. Süreçler ve aralarındaki bağ

İki ayrı süreç var ve aralarındaki **tek bağ bir klasör**.

```text
SÜREÇ 1 · Python uçuş
  scenario.yaml → bvrgym_sim.py → BaseBVRAgent ×2
                                   ├ F16BVR + PID autopilot
                                   └ AAMBVR ×2 + PN güdüm        (6 ayrı FGFDMExec, dt 8,33 ms)
                                 → recorder.Recorder              (ham property, hesap yok)
                                 → time.sleep(sleep_s)
        ↓ CSV yazımı, ~12 Hz snapshot
  recordings/<oturum>/            ← disk, tek buluşma noktası
    <nesne>.csv · events.jsonl · manifest.json
        ↓ klasör imzası (ad+boyut+mtime), 0,25 s yokla / 2 s sakinlik
SÜREÇ 2 · Görüntüleyici
  RecordingStore → server.py (127.0.0.1:8765) → tarayıcı web/
        ← POST yalnızca dizin ve kayıt seçimi
```

Geri yönde **komut yolu yok**: arayüz simülasyona yazamaz, simülasyon arayüzü tanımaz. Kayıt bitmeden canlı akış da olmaz, çünkü okuyucu klasör 2 saniye sakinleşene kadar bekler ve sonra klasörü baştan okur.

## 2. Diğer node'larla paylaşılan veri: yok

| Kanal | Durum |
|---|---|
| UDP durum yayını | yok |
| Çerçevelenmiş TCP komut / olay kanalı | yok |
| JSBSim native output (SOCKET / CSV / FLIGHTGEAR) | `data_output_xml = None` (her iki config) |
| ACMI yazımı veya Tacview canlı protokolü | yok |
| C++ paketleri ve komut yöneticisi | kod yok |
| Simülasyona dış komut girişi | yok; komutlar döngüde sabit |
| Görüntüleyici → tarayıcı | `GET 127.0.0.1:8765/api/state`, 2000 örneklik JSON parçaları |
| Tarayıcı → görüntüleyici | `POST /api/directory`, `/api/choose-directory`, `/api/open` |

Kod tabanında uçuş verisi taşıyan tek bir socket yoktur.

## 3. Bir döngü adımında ne oluyor

Ölçülen değerler: `dt = 0,00833333 s` (120 Hz), `Control_time_step = 10`, `scenario.yaml` içindeki `sim_time_step = 1` (BVRGym varsayılanı 6).

```text
blue uçak → blue füzeleri → red uçak → red füzeleri → record() → sleep(0,08)
   apply_action()              apply_action()           tek snapshot
   └ F16BVR.step(): Sim_time_step × _step()
       └ _step(): hata bir kez okunur, Control_time_step=10 kare boyunca sabit tutulur
```

- Bir dış adım **83,3 ms** simülasyon ilerletir; bunun **tek karesi** kaydedilir → fizik 120 Hz, kayıt 12 Hz.
- Sleep kapalıyken 3,08 s simülasyon 0,04 s duvar saatinde koştu: fizik ≈77× hızlı. Gerçek zamanı tutan tek şey `sleep_s`.
- Uçaklar ve füzeler **sırayla** ilerletiliyor; blue füzesi, bu adımda henüz ilerletilmemiş red uçağını hedef olarak okuyor.

## 4. Doğrulanmış hatalar

### 4.1 Latitude sütunu jeodezik, okuyucu jeosantrik bekliyor

`recorder.py` `Latitude` sütununa `position/lat-geod-deg` yazıyor; `recordings.py` o sütunu jeosantrik kabul edip JSBSim'in radyal irtifasıyla eşleştiriyor.

| Ölçüm | Değer |
|---|---|
| `position/lat-gc-deg` | 59,0000225 |
| `position/lat-geod-deg` | 59,1694424 |
| Mutlak konum hatası | **18 837 m** |
| 28,2 km ayrımda bağıl hata | **−157 m** (%0,56) |
| Veri kaybı | yok; iki ham sütun da kayıtta |

Hata kayıtta değil, sütun takma adında. Öneri: `Latitude` tekrar `position/lat-gc-deg` olsun, jeodezik dönüşüm ACMI ihracatında tek yerde yapılsın.

### 4.2 Füze Mach değeriyle fırlatılıyor

`AAMBVR.launch()` taşıyıcının `get_mach()` çıktısını `vel` olarak geçiyor; `FDMObject.reset()` bunu m/s sayıp `ic/u-fps = m2f(vel)` yapıyor.

| Verilen `vel` | Sonuç |
|---|---|
| 0,9735 (mevcut kod, Mach) | u = 3,19 ft/s → **1,0 m/s**, Mach 0,0032 |
| 300,0 (doğru, m/s) | u = 983,82 ft/s → 299,9 m/s, Mach 0,9733 |

Kaynak: `BVRGym/jsb_gym/simObjects/missiles.py:64`. BVRGym'in kendi hatası; atış hiç denenmediği için pratikte tetiklenmedi.

### 4.3 `is_target_lost()` mantığı ters, sayaç yanlış alana yazıyor

`missiles.py:100-114` içinde `if self.target_norm_old > self.target_norm:` → mesafe **küçülürken** (füze yaklaşırken) `lost_count` artıyor; kaybetme koşulu tersine çevrilmiş. `else` dalı da `self.count_lost = 0` yazıyor, yani `lost_count` hiç sıfırlanmıyor. Çağrı yeri `step()` içinde yoruma alınmış, bu yüzden bugün etkisi yok. Kendi ıskalama tespitimizi yazarken bu fonksiyon referans alınmamalı.

### 4.4 `sim_time_step = 1` füze mesafe geçmişini düzleştiriyor

`AAMBVR.step()` önce `target_norm_old`'u, sonra iç döngüde `target_norm`'u hesaplıyor. Tek iterasyonda ikisi aynı anın ölçümü olur; kayıttaki iki sütun birbirinin kopyası hâline gelir. Hit/lost kontrollerinin **frekansı değişmez** (her iki ayarda 83,3 ms'de bir), değişen tek şey `target_norm_old`'un ne kadar geriden örneklendiğidir.

## 5. Farklar ve eksikler

| # | Konu | Durum |
|---|---|---|
| 5.1 | **Ölçülen hız kayıtta var, arayüzde tahmin görünüyor** | Recorder `velocities/vt-fps`'i ham yazıyor, arayüzün beklediği `TAS` (m/s) sütununu yazmıyor. Arayüz konumdan türetiyor: 297,65 m/s gösterdi, gerçek 300. Çözüm dönüşümü gösterim katmanına taşımak. |
| 5.2 | **Taşıyıcı kimliği dosya adında var, arayüz yine tahmin ediyor** | Recorder `AIM-120 AMRAAM (KE--blue--0) [Blue].csv` yazıyor; arayüz hâlâ eski logger biçimine göre sahipliği renkten çıkarıyor. Mimarideki "taşıyıcı kimliği yok" sınırı bu kayıt için geçersiz. |
| 5.3 | **Komutlar hiçbir yere kaydedilmiyor** | Uygulanmış gaz (`fcs/throttle-cmd-norm`) var; komut edilen heading/irtifa yok. Kararların sonucunu değerlendirmek projenin amacı olduğundan en kritik eksik. |
| 5.4 | **Olaylar zaman damgalı değil** | `state/` altındaki bayraklar doğru ve hesapsız, ama "şu anda fırlatıldı / isabet" olay kaydı yok. Arayüz `state/` klasörünü hiç okumuyor (`glob('*.csv')` alt klasöre inmez). |
| 5.5 | **Tacview uyumluluğu doğrulanmamış** | Kök CSV'ler Tacview sütun adlarını taşıyor ama üstüne 67 ham sütun eklenmiş; gerçek Tacview biçimi ACMI. Ters yön de yok: arayüz `.acmi` açamıyor. |
| 5.6 | **JSBSim script bitişi kontrol edilmiyor** | Uçak scripti `end="700"`, füze scripti `end="200.008"`. Ölçüldü: bitişte `fdm.run()` **False** döner ama **saat ve fizik ilerlemeye devam eder** (200,008 → 300,02 s). Yani süre sert bir sınır değil; gerçek sorun dönüş değerinin hiç kontrol edilmemesi — gerçek bir hata da aynı şekilde sessiz kalır. |
| 5.7 | **Belgeler kaynağı yerleşik logger sanıyor** | `VERI_AKISI.md` ve `README.md` "değiştirilmeyen yerleşik TacviewLogger" diyor; gerçek kaynak 75 sütunlu kendi recorder'ımız. Silinmiş dosyalara atıf duruyor: `bvrgym_sim_template_yaml_config.py`, `agent_ucus.py`, `ucus.py`. |
| 5.8 | **Takma ad sütunları ham sütunları tekrarlıyor** | 75 sütunun 8'i (`Time, Longitude, Latitude, Altitude, Roll/Pitch/Yaw (deg), Mach`) ham karşılıklarının birebir kopyası. Zararsız; ACMI yazıcı gelince gereksiz kalır. |

## 6. Kayıt içeriği

Güncel hâli (10 Ekim 2026 düzeltmesinden sonra):

| Dosya | İçerik | Alanlar |
|---|---|---|
| `F-16 (KE--blue) [Blue].csv` | Uçak fiziği + uygulanan dış komut, snapshot başına bir satır | 19 sütun |
| `AIM-120 AMRAAM (KE--blue--0) [Blue].csv` | Füze fiziği; başlıkla açılır, atıştan sonra dolar | 17 sütun; `Time` taşıyıcının saati, `simulation/sim-time-sec` füzenin kendi saati |
| `events.jsonl` | `target_assigned`, `target_cleared`, `missile_launched`, `missile_hit`, `missile_lost`, `health_changed` | satır başına bir JSON |
| `manifest.json` | Senaryo, sütun kaynak/birim listesi, koordinat referansı, fizik adımı, nesne listesi | — |

**Kayıtta olan:** konum (jeosantrik enlem + jeodezik enlem ayrı sütun), irtifa, yönelim, Mach, TAS (`velocities/vt-fps`), NED hız bileşenleri, yük katsayısı, yakıt, uygulanan gaz, uygulanan dış komut; zaman damgalı olaylar; oturum manifesti.

**Kayıtta olmayan:** mesafe/closure/ATA/AA gibi geometri, zarf ve değerlendirme sonuçları, plan/rota. Bunlar C++ hesap katmanına ait ve ayrı kayıt dosyaları olarak sonra eklenecek.

**Bilinçli dışarıda:** kuvvet/moment toplamları, atalet ve kütle, atmosfer ve rüzgâr, yer teması, kontrol yüzeyi konumları, gövde hızları, açısal hızlar, alpha/beta, aerodinamik katsayılar, çözücü ayarları, motor iç durumları. Hiçbiri sahnede çizilmiyor veya BVR kararına girmiyor; gerekirse `recorder.py` içindeki `BVR_FIELDS` listesine eklenir.

## 7. Hesaplama nerede yapılıyor

Recorder **hiçbir** birim dönüşümü veya türetme yapmıyor; bu amaçlandığı gibi çalışıyor. Hesap yapan tek yer arayüz:

| Yer | Hesap | Değerlendirme |
|---|---|---|
| `telemetry.py` | Konumdan hız türetme, ISA Mach tahmini | C++ hesap katmanına ait |
| `recordings.py` | ECEF, jeodezik enlem, yerel doğu/kuzey düzlemi | Çizim için gereken gösterim matematiği; arayüzde kalabilir |
| `web/units.js`, `stream.js` | Birim çevrimi, örnekler arası interpolasyon | Gösterim; arayüzde kalır |

Taktik geometri (mesafe, closure, ATA/AA) hiçbir yerde hesaplanmıyor.

## 8. BVRGym'in hazır çıkış yolları

| Yol | Nasıl kurulu | Sınır |
|---|---|---|
| **FlightGear** | JSBSim'in native `FLIGHTGEAR` UDP çıkışı. `conf.data_output_xml` dolu olursa `FDMObject.__init__` içinde `fdm.set_output_directive()` çağrılır. `data_output/flightgear.xml` port 5550, `flightgear_red.xml` port 5551, rate 60 Hz. `fg_sleep_time` iç fizik döngüsünde uyuyarak akışı gerçek zamana yaklaştırır. | **Nesne başına bir UDP portu ve bir `fgfs` süreci.** Tek yön: komut geri gelmez. İçerik FlightGear Native FDM ikili yapısı — konum/yönelim/kumanda; ajan adı, takım, mühimmat ve olay taşımaz. Bizim hesaplarımızı gösteremez, çünkü veri JSBSim'den doğrudan çıkar. |
| **Tacview** | Eklenti değil: `jsb_gym/utils/loggers.py` içindeki `TacviewLogger` ölçümleri RAM'de biriktirir, `save_logs()` klasörü **silip** yeniden oluşturup pandas ile nesne başına CSV yazar. Level 3'te `env.log_tacview()` ile sürülür. | Canlı protokol yok, ACMI yok, olay yok. 7 sütun. Aynı takımda aynı füze numarası aynı dosyaya yazılır. |
| **JSBSim native SOCKET** | `data_output/socket.xml` hazır duruyor (port 1138, rate 20, `<LABELS>` başlıklı virgüllü metin). BVRGym bunu kullanmıyor. | Yalnızca seçilen fizik propertyleri; kimlik/envanter/olay yok. |

Her üçünün de ortak özelliği: **nesne başına ve fizik düzeyinde** çıkış. Hiçbiri senaryo kimliği, takım, mühimmat veya olay taşımaz — bu bilgiyi ekleyen katman bizim tarafımızda olmak zorunda.

## 9. Mimariye göre nerede durduğumuz

Mimari belgedeki 6 aşamadan hiçbiri tamamlanmadı, ama ikisinin malzemesi hazır: kayıt katmanı hesapsız ham veriyi doğru üretiyor, görüntüleyici aynı veriyi oynatıyor. Eksik olan, aradaki sözleşmelerin yazılı olmaması — enlem hatası tam olarak bunun belirtisi: iki ucu da biz yazdık ama hangi referansın taşındığı hiçbir yerde kayıtlı değil.

Tacview konusunda iki yön ayrı iştir: **bizden Tacview'e** gerçek ACMI yazmak, **Tacview'den bize** arayüze ACMI okuyucu eklemek. ACMI, WGS84 jeodezik enlem ve referans noktası istediği için dönüşüm şarttır; ama bu dönüşüm kaydın kendisinde değil, açık bir ihracat adımında yaşayabilir. Ham CSV sayısal gerçeğin kaynağı olarak kalır.
