# Kurulum ve çalıştırma

Komutlar Windows PowerShell için ve proje kökünden verilmiştir. Simülasyon ile C++ otopilot ayrı terminallerde çalışır. Görüntüleyici üçüncü, bağımsız bir programdır.

## 1. Gereken araçlar

| Araç | Kullanım |
|---|---|
| Git | Depoyu edinmek (`BVRGym` alt modülü yalnız eski `src/sim` ve WEZ taraması için gerekir) |
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

Simülasyon `src/simulator`'dır; JSBSim'i doğrudan kullanır, F-16 XML'leri `src/simulator/jsbsim_data/` altındadır. Görüntüleyici tek başına Python standart kütüphanesiyle çalışır; JSBSim ve diğer bağımlılıklar uçuşu çalıştıran ortam içindir.

Ortam etkinleştirme engelleniyorsa Python'u doğrudan çağırabilirsin:

```powershell
.\.venv\Scripts\python.exe .\src\simulator\main.py
```

MinGW araçları PATH içinde bulunmalıdır. Örneğin w64devkit kurulumu kullanılıyorsa onun `bin` dizini eklenir. Makineye özgü mutlak yollar proje komutlarına yazılmaz.

```powershell
g++ --version
cmake --version
```

## 2. C++ programını derle

```powershell
cmake --build src/otopilot/build
```

`build/` henüz yoksa ilk kurulum komutu [otopilot KULLANIM.md](../src/otopilot/KULLANIM.md#doğrulama)'dedir (MinGW boşluklu kullanıcı yolunda olduğu için kısa yol ve `-B` bayrakları gerekir).

Çıktı `src/otopilot/main.exe` dosyasıdır. CMake `control`, `model`, `calculations`, `io`, `common` klasörlerindeki (alt klasörler dahil) `.cpp` dosyalarını toplar; yeni dosya eklendiğinde `CONFIGURE_DEPENDS` listeyi yeniler.

Projede Windows Winsock bağlantısı kullanılır ve `ws2_32` bağlanır. Farklı bir derleyici/generator kullanmak için mevcut yapılandırmayı karıştırmak yerine ayrı build dizini oluştur.

## 3. Senaryoyu ayarla

[scenario.yaml](../src/simulator/scenario.yaml) başlangıç konumlarını, mühimmatı, gazı, süreyi ve TCP adresini tanımlar.

| Alan | Anlam |
|---|---|
| `world.duration_s` | Çalıştırılacak simülasyon süresi, s |
| `world.sleep_s` | Her dış adım sonundaki gerçek zaman beklemesi; `0` beklemeden çalıştırır |
| `world.physics_steps_per_tick` | Bir C++ adımındaki JSBSim adımı (10 × 1/120 s = 1/12 s) |
| `world.sensors` | `false`: düşman hep görünür, RWR boş. `true`: radar (menzil, notch) ve RWR |
| `connection.host`, `connection.port` | Python sunucusunun dinleyeceği adres |
| `agents.blue`, `agents.red` | Mevcut C++ sürümünün beklediği kimlikler |
| `name`, `team`, `type` | Görünen ad, takım ve C++ profil etiketi |
| `ammo` | Slot sırasıyla füze profili ve adedi (`aim120c5: 6`); profil `src/simulator/data/missiles/<ad>.yaml` |
| `spawn` | `lat`, `long`, `heading` derece; `alt` metre; `vel` m/s |
| `throttle` | Başlangıç gaz hedefi, `0..1` |

Profil etiketi `src/otopilot/model/profiles/aircraft_profiles.hpp` içinde tanımlı olmalıdır. Farklı bir `type` etiketi Python'daki fizik modelini değiştirmez.

## 4. Uçuşu çalıştır

Birinci terminal:

```powershell
python .\src\simulator\main.py [--name ad] [--altitude m] [--throttle 0..1] [--range NM] [--duration s]
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

C++ olmadan uçuş denemeleri `src/simulator/tools/` altındaki ölçüm betikleriyledir ([simülatör README](../src/simulator/README.md)). Bu durumda C++ hesaplarını içeren `calculations.csv` üretilmez.

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
| Bilinmeyen füze | `ammo`'daki profilin designation'ı `src/otopilot/model/profiles/missile_profiles.hpp` tablosunda olmalı |
| Python geçersiz uçuş komutunda duruyor | Her uçak için bir komut, yön `0..360`, gaz `0..1`, sonlu sayılar gönder |
| Arayüzde otopilot hesabı görünmüyor | C++ kullanılan kayıt olmalı; `calculations.csv`, uçak kimliği ve tam zaman eşleşmesini kontrol et |
| Harita boş, uçuş görünür | Harita katmanını ve internet erişimini kontrol et; harita uçuş oynatımından bağımsızdır |
| Değişiklik derlemeye yansımıyor | Dosyaları kaydet; doğru proje/build dizinini derlediğini doğrula |

Test komutları ve doğrulama kapsamı [doğrulama rehberindedir](../BVRGym-Goruntuleyici/Dogrulama.md).
