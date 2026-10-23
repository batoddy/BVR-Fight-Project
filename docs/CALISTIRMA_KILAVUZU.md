# Kurulum ve çalıştırma

Komutlar Windows PowerShell için ve proje kökünden verilmiştir. Simülasyon ile C++ otopilot ayrı terminallerde çalışır. Görüntüleyici üçüncü, bağımsız bir programdır.

## 1. Gereken araçlar

| Araç | Kullanım |
|---|---|
| Git ve `BVRGym` alt modülü | Fizik/ajan kaynaklarını edinmek |
| Python 3.11 ve sanal ortam | Simülasyon ve kayıt görüntüleyicisi |
| MinGW g++, CMake 3.20+ ve make | Windows C++17 programını derlemek |
| Güncel tarayıcı | 2D/3D görüntüleyici |
| Node.js | İsteğe bağlı JavaScript testleri; normal görüntüleyici kullanımı için gerekmez |

```powershell
git submodule update --init
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

`BVRGym/jsb_gym/` dolu olmalıdır. BVRGym ayrı bir paket olarak kurulmaz; `bvrgym_setup.py` depo yolunu Python arama yoluna ekler. Görüntüleyici tek başına Python standart kütüphanesiyle çalışır; JSBSim ve diğer bağımlılıklar uçuşu çalıştıran ortam içindir.

Ortam etkinleştirme engelleniyorsa Python'u doğrudan çağırabilirsin:

```powershell
.\.venv\Scripts\python.exe .\src\sim\sim_main.py
```

MinGW araçları PATH içinde bulunmalıdır. Örneğin w64devkit kurulumu kullanılıyorsa onun `bin` dizini eklenir. Makineye özgü mutlak yollar proje komutlarına yazılmaz.

```powershell
g++ --version
cmake --version
```

## 2. C++ programını derle

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles"
cmake --build src/otopilot/build
```

Çıktı `src/otopilot/main.exe` dosyasıdır. CMake ana dizin ile `control`, `model`, `calculations`, `io`, `common` klasörlerindeki `.cpp` dosyalarını toplar. Bu klasörlere yeni kaynak eklendiğinde `CONFIGURE_DEPENDS` listeyi yeniler. `build` ve `third_party` kaynak taramasına katılmaz.

Projede Windows Winsock bağlantısı kullanılır ve `ws2_32` bağlanır. Farklı bir derleyici/generator kullanmak için mevcut yapılandırmayı karıştırmak yerine ayrı build dizini oluştur.

## 3. Senaryoyu ayarla

[scenario.yaml](../src/sim/scenario.yaml) başlangıç konumlarını, mühimmat sayısını, gazı, süreyi ve TCP adresini tanımlar.

| Alan | Anlam |
|---|---|
| `world.duration_s` | Başlangıçtan itibaren çalıştırılacak fizik süresi |
| `world.sleep_s` | Her dış adım sonundaki gerçek zaman beklemesi; `0` beklemeden çalıştırır |
| `world.sim_time_step` | BVRGym'in adım tekrar sayısı; saniye veya fizik `dt` değeri değildir |
| `connection.host`, `connection.port` | Python sunucusunun dinleyeceği adres |
| `agents.blue`, `agents.red` | Mevcut C++ sürümünün beklediği kimlikler |
| `name`, `team`, `type` | Görünen ad, takım ve C++ profil etiketi |
| `ammo` | Python'da oluşturulacak füze sayısı; C++ profilinin `loadout` uzunluğunu aşmamalı |
| `spawn` | `lat`, `long`, `heading` derece; `alt` metre; `vel` m/s |
| `throttle` | Başlangıç gaz hedefi, `0..1` |

Profil etiketi `src/otopilot/model/aircraft_profiles.hpp` içinde tanımlı olmalıdır. Farklı bir `type` etiketi Python'daki fizik modelini değiştirmez.

## 4. Uçuşu çalıştır

Birinci terminal:

```powershell
python .\src\sim\sim_main.py
```

Program kayıt dizinini yazdırır ve C++ istemcisini bekler. İkinci terminal:

```powershell
.\src\otopilot\main.exe
```

Varsayılan C++ adresi `127.0.0.1:6000`'dır. YAML'da adres/port değiştirildiyse C++'a da ver:

```powershell
.\src\otopilot\main.exe 127.0.0.1 6001
```

Süre dolunca Python `end` mesajı gönderir ve dosyaları kapatır. Her çalıştırma `recordings/<zaman>/` altında ayrı kayıt oluşturur. Ctrl+C ile kesilen çalıştırmada kaydedici kapanışını yapar; bağlantının erken kapanması karşı tarafta bağlantı hatası olarak görünebilir.

## 5. TCP olmadan uçuş

```powershell
python .\src\sim\_sim_local_template.py
```

Bu şablon YAML'daki yön/irtifa/gaz hedefleriyle çalışır. Döngü içinde komutlar elle değiştirilebilir ve `simulation.fire("blue", "red")` çağrılabilir. C++ çalışmaz; dolayısıyla C++ hesaplarını içeren `calculations.csv` üretilmez.

## 6. Kaydı aç

```powershell
python .\BVRGym-Goruntuleyici\viewer.py --recording .\recordings
```

`http://127.0.0.1:8765` adresini aç. **Kayıt aç** menüsünden oturum seç. `--recording` doğrudan CSV'lerin bulunduğu bir oturum klasörü de olabilir. Parametre verilmezse izlenen varsayılan dizin `BVRGym/data_output/tacview`, varsa kütüphane dizini proje `recordings/` dizinidir.

```powershell
python .\BVRGym-Goruntuleyici\viewer.py --recording .\recordings --port 8766
```

Kayıt tamamlandıktan sonra açılması önerilir: okuyucu iki saniye değişmeyen dosyaları yükler, fakat uzun bir yazma duraklamasını kesin tamamlanmadan ayıramaz. Normal kayıt oynatımı yereldir; yalnız isteğe bağlı OpenStreetMap katmanı internetten görsel ister.

## 7. Sorun giderme

| Belirti | Kontrol |
|---|---|
| C++ Python'a bağlanamıyor | Önce Python'u başlat; IPv4 ve portun iki tarafta aynı olduğunu doğrula |
| `g++` veya `cmake` bulunamadı | MinGW/w64devkit `bin` yolunu PATH'e ekle; terminali yeniden aç |
| Bilinmeyen uçak tipi | YAML `type` ile C++ profil anahtarını eşleştir |
| Füze sayısı/yük hatası | `ammo` sayısını azalt veya profil `loadout` listesini uzat |
| Python geçersiz uçuş komutunda duruyor | Her uçak için bir komut, yön `0..360`, gaz `0..1`, sonlu sayılar gönder |
| Arayüzde otopilot hesabı görünmüyor | C++ kullanılan kayıt olmalı; `calculations.csv`, uçak kimliği ve tam zaman eşleşmesini kontrol et |
| Harita boş, uçuş görünür | Harita katmanını ve internet erişimini kontrol et; harita uçuş oynatımından bağımsızdır |
| Değişiklik derlemeye yansımıyor | Dosyaları kaydet; doğru proje/build dizinini derlediğini doğrula |

Test komutları ve doğrulama kapsamı [doğrulama rehberindedir](../BVRGym-Goruntuleyici/Dogrulama.md).
