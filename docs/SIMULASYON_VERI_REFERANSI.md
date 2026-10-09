# BVRGym ve JSBSim: anlık veri ve dış komut referansı

**İnceleme: 9 Ekim 2026.** Bu belge Workspace'teki BVRGym kaynaklarına ve Python 3.11.9 / JSBSim 1.3.1 ile yüklenen gerçek modellere dayanır. Bir uçuşun anlık verilerini almak için simülasyonu durdurmak gerekmez. Aynı Python programında nesnelerden doğrudan okuyabilirsin; ayrı programlara aktarım için bir çıkış kanalı kurarsın.

## 1. Dosyalar ve kapsam

| Dosya | İçerik |
|---|---|
| [BVRGYM_VERI_LISTESI.csv](BVRGYM_VERI_LISTESI.csv) | 84 temel fizik, ajan, mühimmat ve türetilmiş veri; tip, birim, kaynak, okuma ifadesi, güncellenme ve aktarım yolları |
| [JSBSIM_F16_PROPERTY_KATALOGU.csv](JSBSIM_F16_PROPERTY_KATALOGU.csv) | Yüklü `f16` modelinin 804 propertysi: 782 okunabilir, 22 yalnızca yazılabilir |
| [JSBSIM_FUZE_PROPERTY_KATALOGU.csv](JSBSIM_FUZE_PROPERTY_KATALOGU.csv) | Yüklü `AIM` füze modelinin 468 propertysi: 450 okunabilir, 18 yalnızca yazılabilir |
| [catalog_summary.json](catalog_summary.json) | Sürüm, model, fizik zaman adımı ve sayım bilgileri |

Kataloglarda `R` okunabilir, `RW` okunabilir/yazılabilir, `W` yalnızca yazılabilir anlamındadır. **W satırları çıkış verisi değildir.** Okunabilen propertylerin tümü başlangıç durumunda okunarak doğrulandı; hata oluşmadı. Bu, her propertynin uçuş boyunca anlamlı veya değişken olduğunu göstermez: modele göre sıfır, sabit, ayar veya başlangıç değeri olabilir.

`snapshot_value` örnek başlangıç değeridir; canlı uçuş kaydı değildir. `unit_hint` tam katalogda property adından çıkarılan ipucudur; açıkça belirtilmiştir. Bilinmeyen birimleri model/XML ve dokümantasyondan doğrula. Yeni uçak/XML veya JSBSim sürümü kullanıldığında katalog değişebilir. Bu liste bu kurulumun tüm kayıtlı propertylerini kapsar; bütün JSBSim modellerinin evrensel listesi değildir.

## 2. Veri hangi katmandan gelir?

| Katman | Verebildiği bilgiler | Nasıl okunur? | Doğal veri biçimi |
|---|---|---|---|
| JSBSim fizik motoru | Konum, irtifa, Euler açıları, doğrusal/açısal hızlar, Mach, hücum açısı, ivme/yük katsayısı, kuvvet, moment, motor, yakıt, rüzgâr, atmosfer, kontrol yüzeyleri ve model ayarları | `agent.simObj.fdm['property']` veya `missile.fdm['property']` | Sayısal property API: `float`; bazı bayraklar sayısal 0/1 |
| BVRGym FDMObject | Sık kullanılan fizik alanları ve SI birimine dönüşümler | `agent.simObj.get_altitude()` gibi getterlar; füze de aynı taban sınıftadır | Genellikle `float` |
| BVRGym ajan | Ad, takım, sağlık, hedef, füze envanteri | `agent.conf`, `agent.healthPoints`, `agent.target`, `agent.ammo` | `str`, `float`, `dict`, Python nesnesi / `None` |
| BVRGym füze | Hazır/aktif/alive, hit/lost, hedef ve hesaplanan hedef mesafesi | `missile.is_active()`, `missile.target_hit`, vb. | `bool`, sayı, nesne / `None` |
| Geometri yardımcıları / senin kodun | İki uçak mesafesi, göreli yön, ENU konumu; yaklaşma hızı, aspect, LOS vb. | İki nesnenin aynı andaki durumundan hesaplama | Sayı, vektör; NumPy değerlerini JSON için dönüştür |
| Tüm senaryo | Bütün uçaklar ve füzeler; ortak zaman ve kimlikler | Senin tuttuğun ajan listesinden snapshot derleme | Özel `dict` / JSON şeması |

JSBSim her FDM nesnesinin fizik durumunu hesaplar. Ajan kimlikleri, takımlar, mühimmat ve taktik olayları otomatik bir ortak dünya paketine birleştirmez. Bu birleşik listeyi BVRGym nesnelerinden sen derlersin. Native property çıktısı tek başına hangi hedefin seçildiğini veya hangi ajan adının kullanıldığını söylemez.

## 3. En çok kullanacağın ölçümler

| Veri | Doğrudan kaynak | Tip ve birim / getter |
|---|---|---|
| Simülasyon zamanı | `simulation/sim-time-sec` | float, s; `get_sim_time_sec()` |
| Enlem / boylam | `position/lat-geod-deg`, `position/long-gc-deg` | float, derece; harita ve jeodezik hesap için jeodezik enlem |
| Jeosentrik enlem | `position/lat-gc-deg` | float, derece; `get_lat_gc_deg()` |
| MSL irtifa | `position/h-sl-meters` | float, m; `get_altitude()` |
| AGL yükseklik | `position/h-agl-ft` | float, ft; ×0.3048 ile m; yer modeliyle sınırlı |
| Roll / pitch / yaw | `attitude/phi-deg`, `theta-deg`, `psi-deg` | float, derece; `get_phi()`, `get_theta()`, `get_psi()` |
| Gerçek hava hızı | `velocities/vt-fps` | float, ft/s; `get_true_airspeed()` m/s verir |
| Kalibre hava hızı / Mach | `velocities/vc-kts`, `velocities/mach` | float, knot / boyutsuz |
| Dünya hız vektörü NED | `velocities/v-north-fps`, `v-east-fps`, `v-down-fps` | float, ft/s; getterlar m/s verir; aşağı pozitif |
| Gövde hız vektörü | `velocities/u-fps`, `v-fps`, `w-fps` | float, ft/s; gövde eksenleri |
| Gövde açısal hızları | `velocities/p-rad_sec`, `q-rad_sec`, `r-rad_sec` | float, rad/s |
| Hücum / yana kayma | `aero/alpha-deg`, `aero/beta-deg` | float, derece |
| Pilot yük katsayıları | `accelerations/n-pilot-x-norm`, `n-pilot-y-norm`, `n-pilot-z-norm` | float, g oranı; eksen ve işaretleri koru |
| Yakıt | `propulsion/total-fuel-lbs` | float, lb; `get_total_fuel()` kg verir |
| Motor itkisi | `propulsion/engine/thrust-lbs` | float, lbf; çok motorlu modellerin adları değişebilir |
| Gaz / kumanda komutları | `fcs/throttle-cmd-norm`, `aileron-cmd-norm`, `elevator-cmd-norm`, `rudder-cmd-norm` | float, normalize; ölçülen yön veya kontrol yüzeyi konumuyla aynı değildir |
| Rüzgâr | `atmosphere/wind-*-fps`, `total-wind-*-fps` | float, ft/s; kuzey/doğu/aşağı bileşenleri |

Tabloda kısaltılmış propertyler aynı satırın başındaki klasör önekini kullanır. Tam, kopyalanabilir adlar CSV dosyalarındadır. Kuvvet, moment, atmosfer, kütle/atalet, aerodinamik katsayı, yer teması ve kontrol sistemi ayrıntıları tam katalogda ilgili `forces/`, `moments/`, `atmosphere/`, `inertia/`, `aero/`, `gear/`, `fcs/`, `systems/` gruplarındadır. `ic/` değerleri başlangıç koşullarıdır; canlı uçuş ölçümü yerine kullanılmamalıdır.

## 4. Python'da doğrudan okuma

Mevcut `agent_ucus.py` içindeki `blue` ve `red` nesneleri için örnek:

```python
def read_aircraft(agent):
    aircraft = agent.simObj
    fdm = aircraft.fdm
    return {
        "name": agent.conf.agent_name,
        "team": agent.conf.team,
        "sim_time_s": aircraft.get_sim_time_sec(),
        "lat_geod_deg": fdm["position/lat-geod-deg"],
        "lon_deg": fdm["position/long-gc-deg"],
        "alt_msl_m": aircraft.get_altitude(),
        "roll_deg": aircraft.get_phi(),
        "pitch_deg": aircraft.get_theta(),
        "yaw_deg": aircraft.get_psi(),
        "tas_mps": aircraft.get_true_airspeed(),
        "mach": aircraft.get_mach(),
        "velocity_ned_mps": [aircraft.get_v_north(),
                             aircraft.get_v_east(), aircraft.get_v_down()],
        "fuel_kg": aircraft.get_total_fuel(),
        "health": agent.healthPoints,
        "ready_missiles": sum(m.is_ready_to_launch()
                              for m in agent.ammo.values()),
    }

# Döngüde iki uçağı ilerlettikten sonra:
blue.apply_action(blue_command)
red.apply_action(red_command)
state = [read_aircraft(blue), read_aircraft(red)]
```

Bu okuma ayrı UDP, dosya veya seri port gerektirmez. `state` doğrudan karar koduna verilebilir. Füzelerde fizik nesnesi doğrudan `missile` olduğundan `missile.fdm[...]` ve `missile.get_mach()` kullanılır. Aktif olmayan füzenin fizik durumu uçan füze ölçümü olarak yayınlanmamalıdır; hazır mühimmat ile uçan füze ayrılmalıdır.

Katalog erişimi:

```python
catalog = blue.simObj.fdm.get_property_catalog()  # list[str]; erişim işaretleri dahil
time_s = blue.simObj.fdm.get_property_value("simulation/sim-time-sec")
physics_dt_s = blue.simObj.fdm.get_delta_t()
```

Property adını katalogla doğrula. Yazım hatasını veya olmayan bir alanı sıfır değerli geçerli telemetri sanma. R/W izinleri, komut aralığı ve modelin propertyyi ne için kullandığı farklı kavramlardır.

## 5. Aktarım seçenekleri: doğrudan, UDP, TCP, dosya, seri port

| Yol | Hazır destek / ek iş | Format | Kullanım ve sınır |
|---|---|---|---|
| Aynı süreçte Python | Hazır: nesne/getter/property erişimi | float, dict, list, nesne | Kendi ajan karar kodun için en sade yol; ağ gerekmez |
| JSBSim native UDP çıkışı | Output XML + BVRGym `data_output_xml` | `SOCKET`: metin, virgülle ayrılmış sayılar; başlıkta `<LABELS>` | Seçtiğin fizik propertyleri; paket kaybolabilir; isim/takım/mühimmat otomatik eklenmez |
| JSBSim native TCP çıkışı | Output XML; alıcı sunucu / bağlantı düzeni gerekir | `SOCKET`: aynı metin biçimi | Sıralı byte akışı; mesajları satır sınırından ayır; gecikme birikebilir |
| Özel Python UDP/TCP köprüsü | Ek yayıncı/alıcı kodu gerekir | Senin belirlediğin JSON veya ikili şema | Ajan + füze + göreli geometriyi birleştirir; komutları ayrı doğrulayarak uygular |
| JSBSim native CSV / TABULAR | Output XML | Time ve seçilen sayısal sütunlar; CSV veya sekmeli metin | Disk kaydı; yüksek frekansta flush/okuma gecikmesi canlı kontrol için değerlendirilir |
| JSBSim FLIGHTGEAR | Native output türü | FlightGear Native FDM ikili protokolü | FlightGear görselleştirmesi; JSON/CSV ile aynı değildir |
| BVRGym TacviewLogger | Mevcut logger | Ayrı uçak/füze CSV dosyaları | Mevcut sürümde RAM'de birikir, `save_logs()` çağrısında diske yazılır |
| HTTP/WebSocket | Özel sunucu/köprü gerekir | Genellikle JSON | Tarayıcı / arayüz; mevcut viewer HTTP durumu simülasyon komut köprüsü değildir |
| Standart çıktı / pipe | `print` veya native CSV COUT; özel protokol düzeni | Metin; istenirse satır başına JSON | Konsol loglarıyla karıştırma; `print_status()` yapılandırılmış tam telemetri değildir |
| Fiziksel seri port COM | BVRGym/JSBSim'in bu kurulumunda hazır output değil; ör. pyserial adaptörü gerekir | JSON satırı veya senin ikili çerçeven | Baud, çerçeveleme ve tamponlama eklenir; donanım bağlantısı için |

“Düz” erişimden kastın Python içinden okumaksa ilk satır yeterli. Fiziksel seri port kastediliyorsa son satırdaki adaptör gerekir. Ağ kullanmak zorunlu değildir.

### Native UDP örneği

Bu, oluşturabileceğin XML örneğidir; mevcut uçuş dosyasına uygulanmadı:

```xml
<?xml version="1.0"?>
<output name="127.0.0.1" type="SOCKET" protocol="UDP"
        port="5600" rate="30" precision="12">
  <property>position/lat-geod-deg</property>
  <property>position/long-gc-deg</property>
  <property>position/h-sl-meters</property>
  <property>attitude/phi-deg</property>
  <property>attitude/theta-deg</property>
  <property>attitude/psi-deg</property>
  <property>velocities/vt-fps</property>
  <property>velocities/mach</property>
</output>
```

`config.data_output_xml = str(xml_path)` ayarı **F16BVR oluşturulmadan önce** verilir. Zaman sütununu output sistemi ekler. Sütun sırası XML sırasıdır. Başlık başlangıçta/restart sırasında gönderilir; sonradan açılan UDP alıcısı başlığı kaçırabilir, bu yüzden şemayı alıcıda da bil. Bu çıktı JSON değildir; JSBSim'in metin satırıdır. `vt-fps` burada hâlâ ft/s'dir.

İki uçak için farklı output XML/port kullanmak nesneleri ayırmanın basit yoludur: örneğin 5600 ve 5601. Port→ajan eşlemesini sen tutarsın. Bir porta birden çok nesne kimliksiz sayı paketi göndermek ayırt etmeyi zorlaştırır. Aynı portu iki alıcıyla bağlamaya çalışmak Windows'ta 10048 hatasına yol açabilir.

## 6. Çıkış verisi ile komut girişini ayır

UDP output açmak, o porttan komut alındığı anlamına gelmez. JSBSim'in native input desteği ayrı yapılandırılır; property düzeyindeki girişler ajan adı, hedef seçimi ve `launch_missile()` işlemini kendiliğinden temsil etmez. BVRGym'in PID denetleyicisi de doğrudan yazılan kumanda değerlerini sonraki adımda değiştirebilir.

Senin amaçladığın akış:

```text
JSBSim fizik durumu + BVRGym ajan/füze durumu
                  ↓ snapshot
         senin karar / kontrol programın
                  ↓ açık komut
  hedef yön / irtifa / gaz / hedef ID / ateşleme
                  ↓ simülasyonun döngüsünde uygula
       BaseBVRAgent → F16BVR / AAMBVR → JSBSim
```

Aynı programda `apply_action([heading_deg, altitude_m, throttle])`, `set_target(target_agent)` ve `launch_missile()` çağrıları kullanılır. Ayrı programdan geliyorsa mesajı okuyup ajan ID'sine eşleyen bir köprü gerekir. Bu referans köprüyü kurmaz; mevcut uçuş kodunun davranışını değiştirmez.

Özel ağ protokolünde en az `schema_version`, artan `sequence`, ortak `sim_time_s`, ajan/füze ID'si ve birimli alan adları bulunmalı. Nesne referansını ID, `None` değerini JSON `null`, NumPy scaları Python sayı, tuple/vektörü JSON liste olarak gönder. Ölçülen yön ile istenen yönü ayrı alanlar yap. Füze ID'si taşıyıcıyı da içermeli: ör. `Kartal-1/missile/0`.

## 7. Güncellenme hızı ve zaman

- Yüklü F16 ve AIM modellerinin fizik zaman adımı yaklaşık **0.00833333 s**: simülasyon saniyesi başına yaklaşık 120 fizik adımı. Bu, duvar saatinde otomatik 120 Hz çalıştıkları anlamına gelmez.
- `Sim_time_step` fizik `dt` değeriyle aynı değildir. BVRGym'in döngü tekrarını etkiler; bir `apply_action()` birden fazla `fdm.run()` çağırabilir. Kontrol alt adımlarıyla birlikte dış döngünün ilerlettiği süreyi belirler.
- Anlık okuma zamanı için `get_sim_time_sec()` farkını ölç. `sleep` duvar saatinde bekletir; fizik adım büyüklüğünü değiştirmez.
- Native output `rate` simülasyon zamanına göre örnekleme ister; fizik adımıyla sınırlıdır. Her dış döngüde Python snapshot almak farklı bir örnekleme hızıdır. Aynı snapshotı sık göndermek yeni ölçüm üretmez.
- İki uçak ilerletildikten sonra ortak snapshot al. Her modelin zamanını kontrol et. Füzenin kendi saati launch/reset sonrasında sıfırdan başlayabilir; ortak senaryo zamanını ve füze uçuş süresini ayrı tut.
- Ağ alıcı thread'i JSBSim nesnesini eşzamanlı değiştirmesin. Komut kuyruğuna koy; simülasyon thread'i adım sınırında uygulasın ve okuduğu durumun kopyasını yayınlasın.
- Paket gelmediğinde veri yaşını/son sıra numarasını takip et. Son konumu yeni simülasyon ölçümü gibi göstermemek gerekir.

## 8. Mevcut kayıt ve model sınırları

Şimdiki `src/agent_ucus.py` şablonunda `data_output_xml=None`; native ağ çıktısı kapalıdır. `TacviewLogger` sütunları: `Time, Longitude, Latitude, Altitude, Roll (deg), Pitch (deg), Yaw (deg)`. Zaman s, konum derece, irtifa m, açılar derecedir. Dosya adı ajan configinden türetilir: `uçak_adı (ajan_adı) [takım].csv`; füze kayıtlarında `AIM-120 AMRAAM (AIM{anahtar}) [takım].csv` kalıbı kullanılır. Dosya etiketini gerçek XML model adıyla karıştırma.

Logger kayıtları `save_logs()` çağrıldığında yazar; mevcut şablonda uçuş sonunda veya Ctrl+C sonrasında. Bu sebeple mevcut CSV viewer, canlı dış karar programının telemetri kanalı yerine geçmez. Viewer'ın `/api/state` JSON'u görselleştiriciye aittir; tam fizik katalog çıktısı değildir. Logger seçilen kayıt klasörünü temizleyerek yeniden yazdığı için sürekli ara kayıtta davranışı ayrıca düzenlemek gerekir. Aynı takımda aynı füze anahtarlarının dosya adları çakışabilir.

Mevcut kaynağa özgü ayrıntılar:

- `get_lat_gc_deg()` jeosentrik enlem verir; geospatial yardımcıları bunu jeodezik dönüşüm fonksiyonuna geçiriyor. Hassas yeni hesaplarda `position/lat-geod-deg` kullanıp aynı koordinat tanımını koru.
- `FDMObject.get_u()` şu anda öneksiz `u-fps` okuyor. Gövde ileri hızı için katalogdaki `velocities/u-fps` kaynağını kullan.
- `AAMBVR.launch()` hız için `get_mach()` değerini `reset(..., vel=...)` içine veriyor; reset ise hızı m/s kabul ediyor. Bu kaynak uyuşmazlığı giderilmeden başlangıç füze hızının taşıyıcının m/s hızı olduğunu varsayma.
- `active` işareti ile `is_active()` sonucu farklıdır; ikincisi alive/hit/lost durumlarını da dikkate alır. `target_norm` son hesaplanan mesafedir, doğrudan JSBSim propertysi değildir ve başlangıçta `None` olabilir.
- `lost_count` kaynakta vardır; mesafeye dayalı `is_target_lost()` çağrısı mevcut `step()` içinde yorum satırıdır. Bu sayacı etkin bir hedef takip sensörü gibi yorumlama. Mach/irtifa koşulları da lost sonucuna yol açabilir.
- Vuruş/sağlık BVRGym'in basitleştirilmiş olay mantığıdır. Radar tespiti, RCS, radar track kalitesi, kapsamlı hasar, WEZ/NEZ veya DOR/DR/MAR bu katalogda kendiliğinden hazır birer ölçüm değildir; ilgili model/hesap ayrıca gerekir.

## 9. Kaynaklar

Yerel kaynaklar: `Workspace/BVRGym/jsb_gym/simObjects/FDMObject.py`, `aircraft.py`, `missiles.py`, `jsb_gym/agents/agents.py`, `jsb_gym/utils/geospatial.py`, `jsb_gym/utils/loggers.py`; mevcut uçuş: `Workspace/src/agent_ucus.py`; viewer: proje kökündeki `BVRGym-Goruntuleyici`.

Resmî belgeler ve uygulama kaynakları:

- [JSBSim Python FGFDMExec API](https://jsbsim-team.github.io/jsbsim/python/FGFDMExec.html): property kataloğu, okuma/yazma, zaman ve model erişimi.
- [JSBSim FGOutput](https://jsbsim-team.github.io/jsbsim/classJSBSim_1_1FGOutput.html): output türleri ve XML grupları.
- [JSBSim FGOutputSocket uygulaması](https://jsbsim-team.github.io/jsbsim/FGOutputSocket_8cpp_source.html): UDP/TCP seçimi, LABELS başlığı ve metin veri biçimi. Eski sınıf açıklamasındaki SOCKET uyarısı yerine mevcut uygulama kodu esas alınmıştır.
- [JSBSim FGInputSocket uygulaması](https://jsbsim-team.github.io/jsbsim/FGInputSocket_8cpp_source.html): native giriş çıkıştan ayrı bir mekanizmadır.

Çevrimiçi Python belgesi incelenirken 1.3.2 gösteriyordu; katalog ve yerel davranış doğrulaması bu bilgisayardaki **1.3.1** üzerinden yapıldı. Bu incelemede seri port, yeni ağ köprüsü veya yeni input protokolü kurulup test edilmedi.
