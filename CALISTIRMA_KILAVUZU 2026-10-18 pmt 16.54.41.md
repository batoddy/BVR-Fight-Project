# Çalıştırma kılavuzu

**17 Ekim 2026.** Projeyi sıfırdan kurmak, derlemek, çalıştırmak, kaydı izlemek ve test etmek için
gereken her şey. Komutların hepsi PowerShell'de, **Workspace kökünden** (`BVR-Fight-Project/`)
çalıştırılır.

Kodun ne yaptığı: [src/otopilot/README.md](../src/otopilot/README.md) (C++),
[src/sim/README.md](../src/sim/README.md) (Python), [mimari](mimari/OTOPILOT_MIMARISI.md).

---

## 0. Neler çalışıyor

| Program | Dil | Ne yapar | Başlatma |
|---|---|---|---|
| Simülasyon | Python | BVRGym/JSBSim fiziği, TCP sunucusu, kayıt | `python .\src\sim\sim_main.py` |
| Otopilot | C++ | Ölçümü okur, hesaplar, fighter'ı sürer | `.\src\otopilot\main.exe` |
| Görüntüleyici | Python + tarayıcı | Kayıtları 2B/3B oynatır | `python .\BVRGym-Goruntuleyici\viewer.py` |

Tipik bir oturum:

```
derle (C++)  →  Terminal 1: simülasyon  →  Terminal 2: otopilot  →  uçuş biter  →  görüntüleyicide kaydı aç
```

---

## 1. Bir kez yapılacak kurulum

### 1.1 Gerekenler

| Araç | Bu makinede | Kontrol |
|---|---|---|
| Git | kurulu | `git --version` |
| Python 3.11 | 3.11.9 | `py -3.11 --version` |
| w64devkit (g++, CMake, make) | g++ 16.2, CMake 4.4.4, GNU Make 4.4.1 | `g++ --version`, `cmake --version` |
| Node.js | **kurulu değil** | yalnız görüntüleyicinin JS testleri için gerekir; çalıştırmak için gerekmez |

w64devkit `%LOCALAPPDATA%\Programs\w64devkit\bin` klasöründedir ve PATH'te olmalıdır.
`cmake` ya da `g++` bulunamıyorsa: **Windows ayarları → Ortam değişkenleri → Path**'e bu klasörü ekle
ve terminali kapatıp yeniden aç.

> Eski belgelerdeki `M:\bin\g++.exe` ve `subst M:` komutları artık kullanılmıyor.

### 1.2 Depo ve BVRGym alt modülü

BVRGym ayrı bir git deposudur (alt modül). Yeni bir kopyada içi boş gelir:

```powershell
git submodule update --init
```

`BVRGym\jsb_gym` klasörü görünüyorsa tamamdır. Python, BVRGym'i kurulum yapmadan
klasöründen yükler (`pip install` gerekmez).

### 1.3 Python ortamı

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

Aktif ortamda komut satırının başında `(.venv)` görünür. Bundan sonraki bütün `python` komutları
bu ortamda çalışmalıdır.

`Activate.ps1` "running scripts is disabled" hatası verirse iki yol var:
- Bir kez: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`
- Ya da etkinleştirmeden, ortamın Python'unu doğrudan çağır: `.\.venv\Scripts\python.exe .\src\sim\sim_main.py`

### 1.4 C++ yapılandırması (CMake)

`build/` klasörünü ve Makefile'ı oluşturur:

```powershell
cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles"
```

- `-S`: kaynak klasörü (`CMakeLists.txt`'nin olduğu yer)
- `-B`: derleme klasörü (ara dosyalar burada; `.gitignore`'da)
- `-G "MinGW Makefiles"`: w64devkit'in make'iyle derle

Bu adımı tekrarlaman gereken durumlar: `CMakeLists.txt` değişti, `build/` silindi ya da farklı bir
generator denendi. Yeni `.cpp` eklemek için **gerekmez**.

---

## 2. Her çalıştırmada

### 2.1 Derle

```powershell
cmake --build src/otopilot/build
```

Sonuç `[100%] Built target main` ve `src/otopilot/main.exe`. Yalnız değişen dosyalar yeniden derlenir.

- Derlemeden önce dosyaları kaydet (`Ctrl+K S`). Kaydedilmemiş dosyayı derleyici görmez.
- `main.exe` çalışırken derleme exe'nin üstüne yazamaz; önce programı kapat.
- Her şeyi baştan derlemek için: `cmake --build src/otopilot/build --clean-first`

### 2.2 Simülasyonu başlat (Terminal 1)

```powershell
.\.venv\Scripts\Activate.ps1
python .\src\sim\sim_main.py
```

Beklenen çıktı:

```
Kayıt: C:\...\recordings\20261017_140319_194599
C++ bağlantısı bekleniyor: 127.0.0.1:6000
```

Python burada C++'ı bekler.

### 2.3 Otopilotu başlat (Terminal 2)

```powershell
.\src\otopilot\main.exe
```

Beklenen çıktı: önce `Python'a baglandi: 127.0.0.1:6000`, sonra saniyede bir durum satırı:

```
t  12 s | blue  lat 59.1990  lon 18.4290  alt 5004 m  hdg 26.1  tas 305 m/s  rng 0.0 km  brg 0.0  msl 4 | red ...
```

Süre dolunca iki taraf da kapanır:

```
C++    : Simulasyon bitti. Son tick: 2881
Python : Bitti. 2882 snapshot, 0 olay.
```

**Sıra önemli:** önce Python, sonra C++. C++ önce açılırsa
`Hata: Python'a baglanilamadi (...). Once sim_main.py'yi baslat.` der ve kapanır.

Port 6000 değilse (bkz. 3. bölüm) C++'a da ver: `.\src\otopilot\main.exe 127.0.0.1 6001`.

Uçuşu yarıda kesmek: Python terminalinde `Ctrl+C`. Kayıt düzgün kapatılır
(`Simülasyon durduruldu; kayıtlar kapatıldı.`), C++ bağlantı kapandı hatasıyla çıkar.

### 2.4 Kaydı izle

```powershell
python .\BVRGym-Goruntuleyici\viewer.py
```

Tarayıcıda **http://127.0.0.1:8765** adresini aç. **Kayıt aç** menüsünden `recordings/` altındaki
son klasörü seç. Füze atışları ve sonuçları için **Görünüm → Füze vuruş bilgisi** kutusunu aç.

- Başka port: `--port 8766`
- Belirli bir kaydı açarak başla: `--recording ".\recordings\20261017_140319_194599"`
- Kapatmak: `Ctrl+C` (yalnız görüntüleyiciyi kapatır)

Bir kayıt klasöründe neler var:

| Dosya | İçerik |
|---|---|
| `F-16 (KE--blue) [Blue].csv` | Uçağın her snapshot'taki durumu |
| `AIM-120 AMRAAM (KE--blue--0) [Blue].csv` | Her füzenin durumu |
| `events.jsonl` | Atış, isabet, kayıp gibi olaylar |
| `manifest.json` | Senaryo kopyası, birimler, referanslar |

---

## 3. Senaryo ayarları

`src/sim/scenario.yaml`. Değiştirdikten sonra C++'ı yeniden derlemek gerekmez; Python'u yeniden başlat.

| Ayar | Anlamı |
|---|---|
| `world.duration_s` | Uçuş süresi, simülasyon saniyesi |
| `world.sleep_s` | Her adım sonunda gerçek bekleme. `0` en hızlı; `0.08` yaklaşık gerçek zaman |
| `world.sim_time_step` | BVRGym adım tekrar sayısı (fizik dt değil) |
| `connection.host`, `connection.port` | TCP adresi. Port değişirse `main.exe`'ye de ver |
| `agents.<id>` | Uçaklar. `blue` = fighter, `red` = bogey (`src/otopilot/roles.hpp`) |
| `agents.<id>.type` | Profil adı. `src/otopilot/aircraft_profiles.hpp` tablosunda olmalı |
| `agents.<id>.ammo` | Füze sayısı |
| `agents.<id>.throttle` | Başlangıç gaz komutu, 0..1 |
| `agents.<id>.spawn` | `lat`, `long` (derece), `alt` (m), `vel` (m/s), `heading` (derece, 0..360) |

Bogey'e her adım bu başlangıç komutu (`spawn.heading`, `spawn.alt`, `throttle`) aynen geri gider.

---

## 4. C++ olmadan uçuş

```powershell
python .\src\sim\sim_local.py
```

Uçaklar YAML'daki başlangıç hedefleriyle uçar, kayıt yine `recordings/` altına düşer.
Komutları değiştirmek için `sim_local.py` içindeki döngüye bak. TCP kullanılmaz; Python ve
BVRGym kurulumunu C++'tan bağımsız denemek için kullanışlıdır.

---

## 5. Testler

| Ne | Komut | Bu makinede |
|---|---|---|
| Görüntüleyici, Python | `cd BVRGym-Goruntuleyici; python -m unittest discover -s tests -p "test_*.py"` | 34 test, OK |
| Görüntüleyici, JavaScript | `cd BVRGym-Goruntuleyici; node --test tests/stream.test.mjs tests/units.test.mjs tests/range.test.mjs` | Node.js yok |
| Görüntüleyici + gerçek BVRGym | `cd BVRGym-Goruntuleyici; python tests\integration_bvrgym.py --bvrgym "..\BVRGym" --output "..\recordings\verification"` | |
| Deneme kaydı (atışlı) | `python BVRGym-Goruntuleyici\tests\ornek_kayit.py` | `recordings/` altına olaylı kayıt |
| Uçtan uca (Python + C++) | 2.2 ve 2.3 | 240 s, iki tarafta "bitti" mesajı |

C++ hesap fonksiyonları için ayrı bir test programı henüz yok. Hesapları denemenin yolu şimdilik
uçuşu koşturup terminaldeki `rng`, `brg` değerlerine bakmak.

---

## 6. VS Code ile

- `.vscode/settings.json` CMake Tools eklentisine kaynak klasörünü (`src/otopilot`) söyler.
  Eklenti kuruluysa alttaki durum çubuğundan **Build** ile derlenebilir. Komut satırı yolu
  (2.1) her zaman çalışır.
- Bütün dosyaları kaydet: `Ctrl+K S`. Otomatik kayıt: **File → Auto Save**.
- Bir adı her yerde değiştirmek: ada sağ tıkla → **Rename Symbol** (`F2`).
- İki terminal yan yana: terminal panelinde **Split Terminal**.

---

## 7. Sorun giderme

### Derleme

| Belirti | Sebep | Çözüm |
|---|---|---|
| `cmake` / `g++` tanınmıyor | w64devkit PATH'te değil | 1.1 |
| `'X' has not been declared` | Dosya kaydedilmemiş ya da include eksik | `Ctrl+K S`; `#include "x.hpp"` |
| `fatal error: x.h: No such file or directory` | Include'daki ad dosya adıyla aynı değil | Adları eşitle (`.h` / `.hpp`) |
| `undefined reference to 'Sinif::fonksiyon'` | Fonksiyon `.hpp`'de bildirilmiş, `.cpp`'de gövdesi yok | Gövdeyi yaz |
| `multiple definition of ...` | Başlıkta gövdesi olan fonksiyon `inline` değil | `inline` ekle |
| `Error: generator : MinGW Makefiles does not match` | `build/` başka generator'la kurulmuş | `build/`'i sil, 1.4'ü tekrarla |
| `cannot open output file main.exe: Permission denied` | `main.exe` hâlâ çalışıyor | Programı kapat, yeniden derle |
| `unused parameter` uyarısı | İçi henüz boş fonksiyon | Hesabı yaz ya da `(void)ad;` |

### Çalışma

| Mesaj | Nerede | Sebep ve çözüm |
|---|---|---|
| `Python'a baglanilamadi (...). Once sim_main.py'yi baslat.` | C++ | Python açık değil ya da port farklı. Önce 2.2. |
| `OSError ... [WinError 10048]` / `Simülasyon durdu: ...` (port) | Python | Önceki simülasyon hâlâ açık. Eski terminali kapat ya da portu değiştir. |
| `Hata: Bilinmeyen ucak tipi: X (aircraft_profiles.hpp'ye ekle)` | C++ | YAML'daki `type` tabloda yok. Tabloya satır ekle ya da tipi düzelt. |
| `Hata: aircraft icinde blue veya red yok` | C++ | YAML'daki ajan anahtarları `roles.hpp` ile aynı değil. |
| `Hata: blue: throttle 0..1 disinda: ...` | C++ | Manevra geçersiz gaz üretti. Manevra kodunu düzelt. |
| `Hata: blue: komut sayi degil (NaN/sonsuz)` | C++ | Hesapta sıfıra bölme ya da tanımsız işlem. |
| `Hata: TCP baglantisi kapandi.` / `Python baglantiyi end mesajindan once kapatti.` | C++ | Python hata verip kapandı; Python terminaline bak. |
| `Simülasyon durdu: Komut türü veya adım numarası yanlış.` | Python | C++ yanlış tick'le cevap verdi. `CommandWriter::write`'a `state.tick` gitmeli. |
| `Simülasyon durdu: ...: heading, altitude ve throttle sonlu sayı olmalı.` | Python | Komut eksik ya da geçersiz; C++ kontrolleri normalde bunu önce yakalar. |
| Python çıktısında `Ä°`, `ÅŸ` gibi bozuk harfler | terminal | Konsol kod sayfası. `chcp 65001` çalıştırıp terminali yeniden kullan. |
| İki taraf da bekliyor, hiçbir şey olmuyor | ikisi | Zaman aşımı yok. Hangi tarafın takıldığına bak; ikisini de `Ctrl+C` ile kapatıp yeniden başlat. |

---

## 8. Dağıtım (zip)

`src` ve görüntüleyici klasörlerini, derleme çıktıları ve önbellekler olmadan ayrı ayrı zip'lemek için:

```powershell
@'
import os, zipfile
SKIP_DIRS = {"build", "__pycache__", "node_modules", ".pytest_cache"}
SKIP_EXT = (".exe", ".pyc", ".o", ".obj")
os.makedirs("dist", exist_ok=True)
for folder in ["src", "BVRGym-Goruntuleyici"]:
    path = os.path.join("dist", folder + ".zip")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(folder):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for f in files:
                if not f.endswith(SKIP_EXT):
                    full = os.path.join(root, f)
                    z.write(full, full.replace(os.sep, "/"))
    print(path)
'@ | python -
```

`@' ... '@` PowerShell'in çok satırlı metin yazımıdır; içindeki Python kodu `python -` ile çalıştırılır.
Kapanış `'@` satırın en başında olmalıdır. Sonuç: `dist\src.zip`, `dist\BVRGym-Goruntuleyici.zip`.

`dist/` `.gitignore`'dadır. Zip'i açan kişi 1. bölümü (Python ortamı, BVRGym alt modülü,
CMake yapılandırması) kendi makinesinde yapmalıdır; BVRGym zip'e dahil değildir.
