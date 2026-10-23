# Doğrulama ve test rehberi

Bu belge tekrar çalıştırılabilir kontrolleri ve doğrulamanın sınırlarını tanımlar. Kullanım için [görüntüleyici rehberi](README.md), sistemin bileşenleri için [mimari](../docs/mimari/BVR_SISTEM_MIMARISI.md) okunur.

## Python testleri

Proje kökünden ve Python ortamı etkin halde:

```powershell
Push-Location BVRGym-Goruntuleyici
python -m unittest discover -s tests -p "test_*.py"
Pop-Location
```

Bu testler CSV okuma/doğrulama, koordinat ve hız dönüşümleri, hesapların tam zamanla eşleşmesi, kayıt değişimi, HTTP, envanter, olaylar ve kayıt silme davranışlarını kapsar. Yerleşik Windows klasör penceresi bazı testlerde taklit edilir; testin geçmesi işletim sistemi penceresinin görsel kontrolü değildir.

## JavaScript testleri

Node.js kuruluysa görüntüleyici dizininde:

```powershell
node --test tests/stream.test.mjs tests/units.test.mjs tests/range.test.mjs tests/basemap.test.mjs
```

Kontroller oynatma saati, interpolasyon, birimler, nesne ölçeği, menzil/analiz yardımcıları ve harita projeksiyonunu kapsar. Normal görüntüleyici çalışması için Node.js gerekmez.

## Tarayıcı kontrolleri

`tests/browser-qa.mjs` ve `tests/review-browser.mjs`, Playwright ile ayrı sunucu ve geçici kayıt açar. Ortamda Node.js, Playwright paketi ve uyumlu Chromium bulunmalıdır. Parametreler bu kurulumdaki gerçek yollarla verilir:

```powershell
node tests/browser-qa.mjs --python "C:\ortam\python.exe" --playwright "C:\paketler\playwright\index.js" --browser "C:\tarayici\chrome.exe"
node tests/review-browser.mjs --python "C:\ortam\python.exe" --playwright "C:\paketler\playwright\index.js" --browser "C:\tarayici\chrome.exe"
```

Bunlar yer tutucu yollardır. Gerçek çalıştırmada mevcut Python, Playwright modülü ve tarayıcı yolları kullanılmalıdır. Testler headless çalışır; gerçek Windows klasör seçimi veya dış harita hizmetinin bütün ağ koşulları ayrıca ele alınır.

Elle incelemede kayıt açma/silme, hatalı kayıtta eski görüntünün korunması, iki görünümde hedef/radar/WEZ katmanları, zaman sarma, klavye, birimler, panel boyutlandırma, dar ekran, tema ve isteğe bağlı harita denenir. Sadece test dosyasının bulunması bu kontrollerin yapıldığı anlamına gelmez.

## Gerçek BVRGym logger kontrolü

Görüntüleyici dizininden:

```powershell
python tests/integration_bvrgym.py --bvrgym "..\BVRGym" --output "..\recordings\verification"
```

Bu kontrol JSBSim/BVRGym gerektirir ve gerçek kayıt dosyaları üretir. Yerleşik logger'la ve loggersız uçuş sonucunu karşılaştırır, Üretilen Örneklerin okunmasını kontrol eder. Projenin özel `Recorder` ve C++ taktik döngüsünün tamamını bunun tek başına doğruladığı varsayılmaz.

`tests/ornek_kayit.py`, arayüzde denenebilecek olaylı bir uçuş üretir; birim testi değildir.

## Python–C++ bütünleşme kontrolü

C++ derlendikten sonra kısa bir senaryoda şu zincir doğrulanır:

```text
Simulation.read_state
→ TCP state
→ Sim::step
→ TCP command
→ Recorder.record_calculations
→ açık atışlar ve Simulation.step
→ Recorder.record
→ görüntüleyici read_recording
```

Başarılı sonuç için komut `tick` değerleri eşleşmeli, iki uçak komutu eksiksiz olmalı, C++ `end` ile normal kapanmalı ve hesap satırları aynı zamandaki uçak ölçümlerine bağlanmalıdır. Kısa koşu bütün taktik aşamaları, uzun menzil performansını veya isabet/kayıp davranışının tamamını sınamaz.

## 20 Ekim 2026 doğrulaması

Bu belgeler hazırlanırken mevcut çalışma ağacında yapılan kontroller:

| Kontrol | Sonuç / kapsam |
|---|---|
| `cmake --build src/otopilot/build` | Başarılı Windows C++ derlemesi |
| Görüntüleyici Python testleri | 37 test, başarılı |
| Geçici dizinde 1 saniyelik gerçek Python–C++ uçuşu | 13 dış adım, 14 snapshot, 26 hesap satırı |
| Aynı kaydın görüntüleyicide okunması | 4 uçmuş nesne, 52 örnek, 4 olay, uyarı yok |
| Ölçülen fizik adımı | `0.00833333 s` |
| Mevcut recorder kolonları | Uçak 18, füze 16; okunabilir model alanlarına bağlı |

Kısa bütünleşme kontrolünde süre yalnız bellekteki senaryo kopyasında değiştirildi; proje `scenario.yaml` dosyası değiştirilmedi. TCP örnekleri aynı koşudaki eşleşen state/command çifti ve kapanış mesajından üretildi. Node.js PATH'te bulunmadığı için JavaScript ve Playwright testleri bu doğrulamada çalıştırılmadı. Tam süreli muharebe, manuel tarayıcı incelemesi ve Tacview uygulamasında açılış bu sonuçların kapsamı dışındadır.
