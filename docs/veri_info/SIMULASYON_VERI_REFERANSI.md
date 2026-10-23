# Simülasyon ve kayıt veri referansı

Bu belge Python–C++ mesajlarını, kayıt dosyalarını ve görüntüleyicinin kullandığı temel alanları tanımlar. Alanların sahibi kaynak kodudur: ölçümler `Simulation.read_state()`, komutlar `CommandWriter`, hesaplar `CalculationWriter`, kayıt şeması `Recorder` tarafından üretilir.

## 1. Kimlikler ve zaman

| Alan / kavram | Anlam |
|---|---|
| Uçak `id` | YAML `agents` anahtarı; mevcut C++ rolleri `blue` ve `red` |
| `name` | Görünen ad; kimlik yerine kullanılmaz |
| `team` | Takım etiketi; görüntüleyici Blue/Red'i tanır, diğerlerini nötr gösterebilir |
| `type` | C++ uçak profil anahtarı; fizik modeli seçimi değildir |
| Füze `id` | Python'da `<owner_id>/missile/<ammo_id>` |
| Füze `target_id` | Fırlatılmış füzenin hedef uçağı; hazır slotta `null` |
| `tick` | Sıfırdan başlayan dış adım sayacı |
| `sim_time_s` | JSBSim'den okunan fizik zamanı; ilk ölçüm tam sıfır olmak zorunda değildir |
| CSV `time_s` | Ölçümün ortak kayıt zamanı; füze dosyasında taşıyıcı uçağın saati |
| `missile_time_s` | Füzenin kendi saati; fırlatmada sıfırlanır |

Uçaklar ve aktif füzeler dış adımda sırayla ilerletilir. TCP mesajı, bütün nesnelerin dış adım sınırında derlenen durumudur; alt fizik adımlarının tek tek yayını değildir.

## 2. Birimler ve koordinatlar

| Veri | Sözleşme |
|---|---|
| Enlem / boylam | Derece; TCP `lat_deg` jeodezik, `lon_deg` JSBSim boylamı |
| Başlangıç enlemi | YAML `spawn.lat`, BVRGym tarafından `ic/lat-gc-deg` alanına yazılır; jeosantriktir |
| İrtifa | Python `get_altitude()` metre; C++ `altitude_ft` ve yeni CSV `alt_ft` feet kullanır |
| Kayıt irtifa referansı | Manifest `WGS84 ellipsoid height` bildirir; görüntüleyici bunu MSL'ye dönüştürmez |
| TCP `alt_msl_m` adı | Mevcut alan adıdır; isimdeki MSL ekinden ayrı bir jeoit dönüşümü yapılmaz |
| Yön / açı | Derece; `0` kuzey, `90` doğu; TCP heading `0..360`, C++ içi `(-180,180]` |
| Hız | TCP kinematiğinde m/s; C++ ve yeni CSV'de kt. `tas_kt` gerçek hava hızıdır (KTAS) |
| NED hız | `[kuzey, doğu, aşağı]`; TCP m/s, C++ ve yeni CSV kt; aşağı pozitif |
| Mesafe | C++ hesaplarında ve hesap CSV'sinde NM; patlama yarıçapı ft |
| Gaz | `0..1` aralığında hedef komut; ölçülmüş motor gücü değildir |
| Yük faktörü | JSBSim `forces/load-factor`; gövdeye dik yük, düz uçuşta yaklaşık 1 g |

`StateReader` metre/m-s ölçümlerini C++ birimlerine, `CommandWriter` feet irtifa
hedefini Python için metreye çevirir. Yeni kaydedici `position/h-sl-ft` ve
`velocities/vtrue-kts` alanlarını doğrudan okur; NED hızlarını ft/s'den kt'ye
çevirir. Görüntüleyici yeni ft/kt dosyalarını ve eski metre/ft-s dosyalarını
aynı metre/m-s sahne birimine çözer. `lat_gc_deg` ile `lat_deg` aynı enlem
referansı değildir; başlangıç YAML enlemi ilkiyle karşılaştırılır.

## 3. TCP zarfı

```json
{"kind":"state","tick":0,"data":{}}
```

Bu yalnız zarf gösterimidir; gerçek state için aşağıdaki veri alanları gerekir. Her mesaj yalnız `kind`, `tick`, `data` alanlarını taşır. `kind`, `state`, `command` veya `end` olur; `tick` negatif olmayan tamsayı, `data` nesnedir. UTF-8 JSON tek satır olarak yazılır ve `\n` ile biter. Satır sonu dahil sınır 1 MiB'dir. TCP'nin parçalama/birleştirmesi mesaj sınırı olarak kullanılmaz.

Python sunucudur, C++ istemcidir. Sıra `state(t) → command(t)` şeklindedir; Python yanlış tür veya `tick` değerindeki komutu uygulamaz. Süre sonunda `end` gönderilir. Yeniden bağlanma veya bağımsız atış kabul/red mesajı bulunmaz.

Tam örnekler [TCP mesaj klasöründedir](../mimari/mesaj_sablonlari/tcp/README.md).

### State veri alanları

| Alan | İçerik |
|---|---|
| `data.tick` | Dış adımdaki sayacın veri içindeki kopyası |
| `data.sim_time_s` | Referans uçağın fizik saati |
| `data.aircraft[]` | Uçak kimlikleri, sağlık ve ölçüm alanları |
| `data.missiles[]` | Bütün mühimmat slotlarının kimlik, hedef ve durumları |
| `data.flight_commands[]` | Son uygulanan yön/irtifa/gaz; ilk mesajda YAML hedefleri |

Her uçakta `id`, `name`, `type`, `team`, `health` ile aşağıdaki kinematik alanlar bulunur. Füzenin `kinematics` nesnesi de aynı ölçüm alanlarını kullanır:

| Alan | Kaynak / birim |
|---|---|
| `lat_deg` | `position/lat-geod-deg`, derece |
| `lon_deg` | `get_long_gc_deg()`, derece |
| `alt_msl_m` | `get_altitude()`, metre; referans için yukarıdaki tabloya bak |
| `yaw_deg`, `pitch_deg`, `roll_deg` | JSBSim yönelim getterları, derece |
| `tas_mps` | `get_true_airspeed()`, m/s |
| `mach` | `get_mach()`, boyutsuz |
| `velocity_ned_mps` | NED hız getterları, m/s |

Her füze `id`, `owner_id`, `target_id`, `state`, `outcome`, `flight_time_s`, `kinematics` taşır.

| Durum | Sonuç ve fizik alanları |
|---|---|
| `ready` | Hedef, sonuç, uçuş zamanı ve kinematik `null` |
| `in_flight` | Hedef kimliği, füze saati ve kinematik mevcut; sonuç genellikle `null` |
| `ended` | Son kinematik durum ve `hit`/`lost` sonucu bulunabilir; uçuş artık sürmez |

`lost`, BVRGym'in `target_lost` bayrağının karşılığıdır; bağımsız sensör kilidi kaybı ölçümü olarak yorumlanmaz.

### Command veri alanları

| Alan | İçerik |
|---|---|
| `data.aircraft[]` | Her uçak için tek `{id, heading_deg, altitude_m, throttle}` |
| `data.launch[]` | Tek adımlık `{id, target_id}` atış emirleri; yoksa boş liste |
| `data.calculations.aircraft[]` | Uçak başına C++ hesabı; fizik komutu olarak uygulanmaz |

`heading_deg` için `0..360`, `throttle` için `0..1` kabul edilir. Yön, irtifa ve gaz sonlu sayı olmalıdır. Python her ajan için tam bir komut ister. C++ mevcut iki uçağa komut üretir. `launch` belirli füze slotunu seçmez; Python hazır ilk fizik slotunu kullanır.

### End

`kind="end"`, son `tick` ve boş `data` nesnesi gönderilir. C++ yanıt üretmeden ana döngüden çıkar.

## 4. Kayıt dizini

```text
recordings/<oturum>/
├── F-16 (<ad>--<ajan>) [<takım>].csv
├── AIM-120 AMRAAM (<ad>--<ajan>--<slot>) [<takım>].csv
├── events.jsonl
├── calculations.csv
└── manifest.json
```

Her uçak ve slot için ayrı CSV vardır. İsimler kaydedicide dosya sistemi için temizlenir. `calculations.csv` yalnız hesap verisi geldiğinde açılır; yerel uçuş şablonunda bulunmaz.

### Nesne CSV sütunları

| Sütunlar | Kaynak / açıklama |
|---|---|
| `time_s` | Ortak ölçüm zamanı |
| `lat_deg`, `lon_deg`, `alt_ft` | Konum; irtifa `position/h-sl-ft` |
| `roll_deg`, `pitch_deg`, `yaw_deg` | Ham yönelim propertyleri |
| `mach` | `velocities/mach` |
| `lat_gc_deg` | `position/lat-gc-deg` |
| `tas_kt` | `velocities/vtrue-kts`, KTAS |
| `vel_north_kt`, `vel_east_kt`, `vel_down_kt` | NED hızları; kaynak ft/s × `0.3048 × 3600 / 1852` |
| `load_factor_g` | `forces/load-factor` |
| `fuel_lb` | `propulsion/total-fuel-lbs` |
| `cmd_heading_deg`, `cmd_alt_ft`, `cmd_throttle_norm` | Yalnız uçakta; irtifa hedefi m → ft çevrilerek yazılır |
| `missile_time_s` | Yalnız füzede; kendi fizik saati |

Ek property sütunları yalnız modelde okunabiliyorsa açılır. Mevcut modellerle uçak CSV'sinde 18, füze CSV'sinde 16 sütun vardır; bu sayı bütün model sürümleri için sabit protokol değildir. Kesin liste ilgili dosya başlığında ve manifestin `objects[].columns` alanındadır.

Hazır mühimmat yalnız CSV başlığıyla kalır. Uçan füze kayıt alır; bitiş durumu bir kez yazıldıktan sonra satır eklenmez. Snapshot, her fizik alt adımı değil `record()` çağrısıdır.

### Calculations CSV

`time_s`, kaydedici tarafından ilgili uçağın son yazılmış ölçüm zamanı olarak eklenir. C++ hesap satırında `time_s` bulunamaz. Diğer alanlar `CalculationWriter` çıktısıdır:

| Grup | Alanlar |
|---|---|
| Kimlik / aşama | `id`, `target_id`, `phase` |
| Mesafe / açı | `range_2d_nm`, `range_3d_nm`, `bearing_deg`, `ata_deg`, `aspect_deg` |
| Görüş hattı | `los_north_nm`, `los_east_nm`, `los_down_nm` |
| Kapanma | `rate_of_closure_kt`, `enemy_closing_kt` |
| Angajman | `enemy_in_my_wez`, `me_in_enemy_wez`, `mar_nm`, `dor_nm` |
| Profil / çizim | `radar_gimbal_deg`, `radar_range_nm`, `wez_min_range_nm`, `wez_max_range_nm` |

Boolean değerler CSV'de `1/0` olarak yazılır. Alan kümesi ilk mesajdan sonra değiştirilemez. `phase`: `0 Hold`, `1 Fire`, `2 Out`, `3 In`, `4 Crank`, `5 Notch`.

Pozitif ATA sağ taraf, pozitif kapanma yaklaşma anlamındadır. Radar gimbal değeri yarım açıdır: `60`, sağa ve sola 60° demektir. MAR/DOR'un mevcut `0` değerleri uygulanmış bir angajman çözümü değildir. WEZ ilk slotun profiline dayanır. Füzenin `target` hesapları bu uçak satırlarına ayrı füze ölçümü olarak aktarılmaz.

Görüntüleyici hesapları uçak anahtarı ve **tam eşit `time_s`** ile ölçüme bağlar; en yakın zamanı seçmez. Eşleşmeyen hesap uyarıyla atlanır. Oynatma sırasında `calc`, bir sonraki ölçüme kadar önceki örneğin hesabı olarak tutulur; geometrisi yeniden hesaplanmaz. Son ölçüm için hesap olmayabilir.

### Events JSONL

Her satır ayrı bir JSON olayıdır. Temel alanlar: `seq`, `sim_time_s`, `agent`, `type`. Olaya göre `target`, `ammo` veya `from`/`to` eklenir.

| Tür | Ek alan / anlam |
|---|---|
| `target_assigned`, `target_cleared` | `target`; ajanın hedef bağının değişmesi |
| `missile_launched` | `ammo`, `target`; fırlatmanın gözlenmesi |
| `missile_hit`, `missile_lost` | `ammo`, `target`; BVRGym sonuç bayrağı |
| `health_changed` | `from`, `to`; sağlık değişimi |

`agent` kimliktir; olay `target` değeri kaydedicinin tuttuğu görünen hedeftir. Olay zamanı durum değişiminin görüldüğü snapshot'tır. Görüntüleyici ilk 5000 olayı gösterir; daha fazlasını uyarır. Sonuç, kayıt aralığının bitmesinden türetilmez.

### Manifest JSON

Yeni kaydedici şeması `schema_version=3`, `kind="bvrgym-bvr-recording"` olarak bildirilir.
Şema 2'nin `alt_m`, `tas_fps`, NED `_fps` ve hesap `_m`/`_mps` dosyaları eski
oturumlarda korunur ve görüntüleyici tarafından okunur. Manifest senaryoyu,
koordinat referansını, sütun/property/birim/çarpan kataloğunu, nesnelerin dosyalarını,
fizik/kontrol adımlarını ve snapshot/olay/hesap sayılarını içerir. İlk açılışta ve
kapanışta yazılır.

Bu sürüm numarası TCP zarfında bulunmaz ve görüntüleyicinin HTTP `schema_version=3` değeriyle aynı sözleşme değildir.

## 5. Yerleşik TacviewLogger biçimi

Görüntüleyici ayrıca BVRGym logger'ının `Time`, `Longitude`, `Latitude`, `Altitude`, `Roll (deg)`, `Pitch (deg)`, `Yaw (deg)` adlandırmasını okur. Buradaki `Latitude` jeosantrik kabul edilip okuma sırasında jeodeziğe çevrilir. Bizim `lat_deg` sütunumuz doğrudan jeodeziktir.

Yerleşik logger manifest/olay/hesap dosyası üretmek zorunda değildir. Sahiplik önce dosya adındaki ajan anahtarından, bu yoksa takımda tek olası uçak bulunmasından çözülür. Belirsiz sahiplik tahmin edilmez. Yerleşik logger dosya adı çakışmasıyla ölçümleri ezmişse görüntüleyici kaybolan veriyi üretemez.

Görüntüleyicinin CSV okuması, özel recorder sütunlarının Tacview uygulamasınca doğrudan açılacağını garanti etmez. ACMI dönüştürme bu projede uygulanmış bir çıktı yolu değildir.

## 6. Property katalogları

- [JSBSIM_F16_PROPERTY_KATALOGU.csv](JSBSIM_F16_PROPERTY_KATALOGU.csv): oluşturulmuş F-16 modelinin property envanteri.
- [JSBSIM_FUZE_PROPERTY_KATALOGU.csv](JSBSIM_FUZE_PROPERTY_KATALOGU.csv): oluşturulmuş füze modelinin property envanteri.
- [BVRGYM_VERI_LISTESI.csv](BVRGYM_VERI_LISTESI.csv): bu projenin kullandığı ölçüm, komut ve hesap alanlarına toplu bakış.
- [catalog_summary.json](catalog_summary.json): envanterin alındığı kurulum ve okuma özeti.

Property katalogları ölçüm anına ait örnek değer ve erişim işaretlerini içerir; simülasyonun bütün bu alanları TCP'den gönderdiği anlamına gelmez. `R/RW/W` erişim iznidir; birim, geçerli komut aralığı veya fiziksel kullanım garantisi değildir. `ic/` başlangıç koşulları canlı ölçüm yerine kullanılamaz. Model/JSBSim değiştiğinde `get_property_catalog()` ile yeniden üretilmelidir.
