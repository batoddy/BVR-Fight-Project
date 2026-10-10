# C++ / Python kontrol mimarisi

10 Ekim 2026 — Başka sohbette devam etmek için karar notu.

## Mevcut durum ve amaç

BVRGym/JSBSim Python tarafında çalışıyor; dünya ve ajanların başlangıç ayarları `scenario.yaml` üzerinden geliyor. `src/bvrgym_sim.py` simülasyonu yürütüyor, `src/recorder.py` kayıt alıyor.

C++ bağlantısı henüz uygulanmadı. Sohbette örnek kod ve aşağıdaki dosya ayrımı konuşuldu. Bu not, uygulamanın tamamlandığı anlamına gelmez.

Kullanıcı kendi C++ ajan/kontrol algoritmalarını yazacak. Manevra, hedef seçimi ve ateşleme kararları dışarıdan gelecek; Python bu komutları uygulayıp fiziği ilerletecek. BVRGym'in mevcut uçuş kontrolcüleri ve füze güdümü başlangıçta kullanılacak; özel füze güdümü ileride ele alınabilir.

## Kontrol döngüsü

C++ ana döngüyü yönetir. Aynı süreçte gömülü Python ve pybind11 ile doğrudan fonksiyon çağrıları kullanılır.

1. Python'dan bütün uçak ve füzelerin mevcut durumunu oku.
2. C++ `StateMessage` nesnesini doldur.
3. C++ kontrol algoritması hesaplamalarını yapsın.
4. `CommandMessage` nesnesini doldur ve Python'a ver.
5. Python komutları uygulasın, bütün ajanları bir adım ilerletsin ve recorder kaydı alsın.
6. Yeni adımın ölçümüyle döngüyü tekrarla.

C++ hesaplama yaparken simülasyon ilerlemez. Ölçüm ve komut bütün ajanları kapsar; mavi/kırmızı için bağımsız ilerleyen döngüler kurulmaz. Komutun `based_on_tick` alanı, kullanılan ölçümün `tick` alanıyla eşleşir.

Kontrol bağlantısında TCP/UDP veya JSON dosyası üzerinden aktarım kullanılmaz. JSON dosyaları mesaj alanlarını ve örnek değerleri gösteren referans şablonları olarak kalır. Arayüz veriyi izler; ayrı bir simülasyon başlatmaz.

## Planlanan dosya ayrımı

```text
Workspace/
├── scenario.yaml
├── src/
│   ├── bvrgym_sim.py       # Oluşturma, komut uygulama, simülasyonu ilerletme
│   ├── sim_bridge.py       # C++ tarafından çağrılacak Python arayüzü
│   └── recorder.py         # Kayıt
├── cpp/
│   ├── include/
│   │   ├── messages.hpp    # Ölçüm, uçuş komutu ve ateşleme veri tipleri
│   │   ├── PythonBridge.hpp
│   │   └── Controller.hpp
│   ├── src/
│   │   ├── PythonBridge.cpp # Python çağrıları ve veri dönüşümleri
│   │   ├── Controller.cpp   # Kullanıcının kontrol/karar algoritması
│   │   └── main.cpp         # Başlatma ve ana döngü
│   └── CMakeLists.txt
└── docs/mimari/mesaj_sablonlari/
```

`.hpp` dosyaları tipleri ve fonksiyon arayüzlerini, `.cpp` dosyaları uygulamalarını içerir. pybind11/Python ayrıntıları `PythonBridge.cpp` içinde tutulur; `Controller` yalnızca C++ mesaj nesneleriyle çalışır. İleride mavi/kırmızı için ayrı kontrol sınıfları eklenebilir.

## Mesajlar ve arayüz

- Ölçüm: paket/adım numarası, simülasyon zamanı; uçak kimliği, konumu, MSL irtifası, yaw/pitch/roll ve hızı. Füze kimliği, sahibi, hedefi, hazır/havada/sona erdi durumu ve uçuş verileri.
- Komut: esas alınan adım; her uçak için hedef heading, irtifa ve gaz; tek seferlik ateşleme istekleri. Atışın uygulanıp uygulanamadığı ve sebebi döndürülmeli.
- Henüz atılmamış füzenin uçuş verileri boş olur. `reserved` gelecekte kullanılabilir; mevcut BVRGym özelliği olarak varsayılmaz.
- Python arayüzü: `read_state()`, `initial_commands()`, `step(command)`, `finished()` ve kapanış/kayıt yönetimi.

Mevcut JSON şablonları `mesaj_sablonlari` klasöründe uçak başına ayrılmış sürümdedir. Son karar, bunları birleşik ölçüm/komut yapısına uyarlamaktır; henüz yapılmadı.

## Uygulamaya geçerken

Python dosyası import edildiğinde kendi uzun uçuş döngüsünü başlatmamalı. YAML'daki süre, adım ve bekleme ayarları korunmalı; gereksiz manevra/rüzgar/taktik karar eklenmemeli. Mevcut `.venv` paketleri gömülü Python'a bağlanmalı. C++ derleme ayarları ve bağlantı testleri henüz tamamlanmadı.

Ana kullanım şu kadar sade olmalı:

```cpp
PythonBridge simulation("scenario.yaml");
Controller controller(simulation.initial_commands());

while (!simulation.finished()) {
    StateMessage state = simulation.read_state();
    CommandMessage command = controller.calculate(state);
    simulation.step(command);
}
```
