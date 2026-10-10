# C++ / Python kontrol mimarisi

10 Ekim 2026 — Başka sohbette devam etmek için karar notu.

## Önce büyük resmi anlayalım

Python tarafı uçakları ve füzeleri oluşturur, verilen komutlarla simülasyonu ilerletir. C++ tarafı ise o anki durumu okuyup ne yapılacağına karar verir. Örneğin C++ ‘blue uçağı 90 dereceye dönsün, 5000 metreye gitsin, gazı 0.49 olsun’ diyebilir. Python bu hedefleri BVRGym'e verir. Uçak anında o konuma ışınlanmaz; mevcut uçuş kontrolcüsü ve fizik modeliyle hedefe doğru hareket eder.

Burada **C++ yönetir, Python simüle eder**. Python'un ayrıca kendi başına çalışan bir uçuş döngüsü olmayacak. C++ yeni komutu verene kadar simülasyon bulunduğu adımda kalacak.

**Senin asıl çalışma dosyan `Controller.cpp` olacak.** Burada konumları ve hızları okuyacak, mesafe/açı gibi hesaplarını yapacak ve sonraki uçuş komutunu belirleyeceksin. Python bağlantısının ayrıntılarıyla her hesaplamada uğraşmayacaksın.

Bu doküman bir tasarım ve açıklamalı kod notudur. Şu an çalıştırılabilir C++ bağlantısı kurulmuş değildir.

Okuma sırası: önce kontrol döngüsünü ve dosya görevlerini oku; ardından örneklerde `main.cpp`, `messages.hpp`, `Controller.cpp` bölümlerine bak. Python bağlantısının ayrıntılarına en son geçebilirsin.

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

Örneğin Python 42. adımdaki iki uçağın durumunu verir. C++ bu ölçümü okur ve ‘blue için 90 derece/5000 metre/0.49 gaz, red için 180 derece/7000 metre/0.49 gaz’ komutunu hazırlar. Python bu komutu uygulayıp 43. adıma geçer. C++ sonraki döngüde 43. adımın sonucunu okur.

`tick` bu dış döngünün adım numarasıdır; saniye değildir. `sim_time_s` ise fizik modelinin simülasyon zamanıdır. C++ hesaplamasının gerçek hayatta ne kadar sürdüğü bu ikisinden farklıdır.

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

Dosyaları şu şekilde düşün:

| Dosya | Senin açısından anlamı |
|---|---|
| `scenario.yaml` | Uçakların ismi, takımı, mühimmatı ve başlangıç konumları; süre, adım ve bekleme ayarları. |
| `bvrgym_sim.py` | Ajanları oluşturur ve BVRGym'e komut uygular. |
| `sim_bridge.py` | C++'ın Python'a ‘durumu ver’ veya ‘bu komutu uygula’ diyebildiği giriş noktası. |
| `messages.hpp` | Ölçümün ve komutun içinde hangi alanlar bulunduğunu tanımlar. |
| `PythonBridge.cpp` | Python sözlüğünü C++ nesnesine, C++ komutunu Python sözlüğüne çevirir. |
| `Controller.cpp` | Senin manevra, hedef seçimi ve ateşleme hesapların. |
| `main.cpp` | Diğer parçaları sırayla çağıran ana döngü. |
| `recorder.py` | Simülasyondan çıkan verileri kayıt dosyalarına yazar. |
| `CMakeLists.txt` | C++ dosyalarının ve kütüphanelerin nasıl derleneceğini anlatır. |

`.hpp` bir parçanın dışarıdan nasıl kullanılacağını gösterir: hangi fonksiyonları var, hangi veri tiplerini alıyor? `.cpp` ise o fonksiyonların içeride ne yaptığını yazar. Bir sınıfın bu iki dosyaya ayrılması iki ayrı program oluştuğu anlamına gelmez; derlenince aynı C++ programının parçaları olur.

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

## Son karar: doğrudan Python çağrısı

Kullanıcı doğrudan çağrı yöntemini seçti: C++ gömülü Python'u çalıştıracak, ölçüm ve komutlar bellekte aktarılacak. JSON dosyaları yalnızca referans şablonu olarak kalacak. Kontrol aktarımında dosya yazma ve karşı tarafa hazır sinyali gönderme yöntemi kullanılmayacak; senkron fonksiyonun dönüşü işlemin tamamlandığını bildirir.

## Sohbetteki örnek kodlar

Aşağıdaki kodlar tasarım örnekleridir; proje kaynaklarına uygulanmadı ve derlenip test edilmedi. Python örneği mevcut YAML/spawn bölümünün oluşturduğu `agents`, `world`, `commands` değişkenlerini kullanır. C++ örnekleri, konuşulan dosya ayrımına göre aşağıda gruplanmıştır.

Ölçüm örneği şimdilik uçakları içerir. Füze ölçümleri, komut içindeki ateşleme istekleri, recorder ve güvenli kapanış son uygulamada tamamlanacak. `PythonBridge` sınıfının kurucu/yıkıcı ve `Impl` gövdesi henüz yazılmadı; dönüşüm fonksiyonları bunun içinde kullanılacak örneklerdir.

### Python: src/sim_bridge.py

`Simulation` bir simülasyon oturumunu tutan sınıftır. İçinde ajan listesi, dünya ayarları, son komutlar ve adım sayacı bulunur. `self`, Python'da o simülasyon nesnesinin kendisidir. Örneğin `self.agents`, bu nesnenin uçaklarına erişir.

Kodda gelen üç veri şunlardır:

- `agents`: `blue` ve `red` gibi anahtarlarla BVRGym ajanlarına eriştiğimiz sözlük.
- `world`: YAML'daki süre, bekleme ve adım ayarları.
- `initial_commands`: Her uçak için `[heading, altitude, throttle]` başlangıç hedefleri. Mevcut kodda bunlar `commands` değişkeninde tutuluyor.

Örneğin `agents["blue"].simObj` mavi uçağın simülasyon nesnesidir. `agent.simObj` uçak fiziğine, `agent.ammo` ise o ajanın füze nesnelerine erişir. `blue` bir Python kimliğidir; görünen uçak isminin aynısı olmak zorunda değildir.

Mevcut YAML okuma ve ajan oluşturma bölümü korunur; kendi `while` döngüsü kaldırılır. C++ aşağıdaki fonksiyonları çağırarak ilerletmeyi yönetir.

```python
import time


class Simulation:
    def __init__(self, agents, world, initial_commands):
        # Oluşturulmuş ajanları ve YAML ayarlarını sakla.
        self.agents = agents
        self.world = world
        self.commands = initial_commands

        self.tick = 0  # C++ henüz hiçbir ilerletme komutu vermedi.
        # Ortak zaman göstergesi için ilk uçağın simülasyon saatini kullan.
        self.clock = next(iter(agents.values())).simObj
        self.start_time = self.clock.get_sim_time_sec()

    def finished(self):
        elapsed = self.clock.get_sim_time_sec() - self.start_time
        return elapsed >= self.world["duration_s"]

    def read_state(self):
        """Ölçümü oku. Bu fonksiyon simülasyonu ilerletmez."""
        aircraft = []

        # Her uçağın o anki değerlerini aynı ölçümün içine koy.
        for aircraft_id, agent in self.agents.items():
            sim = agent.simObj

            aircraft.append({
                "id": aircraft_id,
                "lat_deg": float(sim.fdm["position/lat-geod-deg"]),
                "lon_deg": sim.get_long_gc_deg(),
                "alt_msl_m": sim.get_altitude(),
                "yaw_deg": sim.get_psi(),
                "pitch_deg": sim.get_theta(),
                "roll_deg": sim.get_phi(),
                "tas_mps": sim.get_true_airspeed(),
            })

        return {
            "packet_no": self.tick,
            "tick": self.tick,
            "sim_time_s": self.clock.get_sim_time_sec(),
            "aircraft": aircraft,
        }

    def initial_commands(self):
        """YAML'daki başlangıç komutlarını C++ tarafına ver."""
        return [
            {
                "id": aircraft_id,
                "heading_deg": values[0],
                "altitude_m": values[1],
                "throttle": values[2],
            }
            for aircraft_id, values in self.commands.items()
        ]

    def step(self, message):
        """C++ komutunu uygula ve bütün uçakları bir adım ilerlet."""
        if message["based_on_tick"] != self.tick:
            raise ValueError("Komut mevcut simülasyon adımına ait değil.")

        actions = {
            item["id"]: [
                item["heading_deg"],
                item["altitude_m"],
                item["throttle"],
            ]
            for item in message["aircraft"]
        }

        if set(actions) != set(self.agents):
            raise ValueError("Her uçak için bir komut verilmelidir.")

        # Her ajan kendi komutunu uygular; BVRGym fiziği ilerletir.
        for aircraft_id, agent in self.agents.items():
            agent.apply_action(actions[aircraft_id])

        self.commands = actions
        self.tick += 1

        # Recorder bağlantısı son uygulamada burada yapılacak:
        # recorder.record(self.agents, self.commands)

        if self.world["sleep_s"] > 0:
            time.sleep(self.world["sleep_s"])

    def fire(self, shooter_id, target_id):
        """Yalnızca C++ açıkça istediğinde ateşle."""
        shooter = self.agents[shooter_id]
        target = self.agents[target_id]

        if shooter is target:
            return False
        if shooter.is_own_missile_active():
            return False
        if not any(m.is_ready_to_launch() for m in shooter.ammo.values()):
            return False

        shooter.set_target(target)
        shooter.launch_missile()
        return True


def create_simulation():
    # agents, world ve commands mevcut YAML/spawn bölümünden gelir.
    return Simulation(agents, world, commands)
```

Python kodundaki fonksiyonları tek tek okuyalım:

| Fonksiyon | Ne yapıyor? | Simülasyonu ilerletiyor mu? |
|---|---|---|
| `__init__` | Oluşturulmuş ajanları saklar, başlangıç zamanı ve adım sayacını ayarlar. | Hayır. |
| `finished` | YAML'daki uçuş süresinin dolup dolmadığını söyler. | Hayır. |
| `read_state` | Her uçağın gerçek mevcut konumunu, yönelimini ve hızını okur. | Hayır. |
| `initial_commands` | YAML'dan gelen başlangıç uçuş hedeflerini C++'a verir. | Hayır. |
| `step` | Verilen uçuş komutlarını bütün ajanlara uygulayıp ilerletir. | Evet. |
| `fire` | Belirtilen ajanın belirtilen hedefe füze atmasını ister. | Atışı başlatır; sonraki uçuş ilerletmeleri `step` ile olur. |
| `create_simulation` | Bu sınıftan bir simülasyon nesnesi oluşturup C++'a döndürür. | Kendi döngüsünü başlatmaz. |

`read_state()` içindeki `return { ... }` bir **Python sözlüğüdür**. JSON'a benzer görünür ama JSON dosyası değildir. C++ bu sözlüğün alanlarını bellekte okur. `aircraft` listesinde her uçak için bir ayrı sözlük vardır.

Ölçümdeki `lat_deg`/`lon_deg` derece, `alt_msl_m` deniz seviyesine göre metre, `yaw_deg`/`pitch_deg`/`roll_deg` derece, `tas_mps` hava kütlesine göre gerçek hız ve birimi m/s'dir. Yaw burun yönünü, pitch burnun yukarı/aşağı eğimini, roll yatışı gösterir. Bunlar **mevcut durumdur**; komuttaki heading ve irtifa ise **ulaşılmasını istediğin hedeflerdir**.

`step()` gelen mesajın doğru adıma ait olduğunu kontrol eder. Ardından her uçak için heading/irtifa/gaz üçlüsünü çıkarır. Bütün uçakların komutları varsa bunları sırayla uygular. En son sayaç artar; YAML'daki `sleep_s` sıfırdan büyükse gerçek zamanda o kadar bekler. Bu bekleme C++ kodunun hesaplama süresinden ayrıdır.

`fire()` hedefe uzaklık veya açı üzerinden otomatik atış kararı vermez. Yalnızca ajanın kendisine ateş etmediğini, hazır füze olduğunu ve BVRGym'in mevcut aynı anda aktif füze kısıtına uyulduğunu kontrol eder. Taktik karar C++'ta verilecek. Dönüşte `True` atış çağrısının uygulandığını, `False` örnekteki kontroller nedeniyle uygulanmadığını belirtir; hedefe isabet edildiği anlamına gelmez.

### C++: cpp/include/messages.hpp

Burada hesaplama yapılmaz. Yalnızca verileri koyacağımız kapların alanları tanımlanır. `struct` birbiriyle ilişkili alanları tek isim altında toplar. Örneğin bir `AircraftState` nesnesinde tek bir uçağın kimliği, konumu ve hızı vardır.

- `AircraftState`: Tek uçağın ölçümü.
- `StateMessage`: O döngüdeki bütün uçakların ölçümü ve ortak zaman/adım bilgisi.
- `AircraftCommand`: Tek uçağa verilecek yön/irtifa/gaz hedefi.
- `CommandMessage`: O döngüde bütün uçaklara verilecek komutlar.

`std::string` yazı, `double` ondalıklı sayı, `std::uint64_t` negatif olmayan büyük tam sayı, `std::vector` ise içinde birden fazla eleman tutabilen listedir. Bu nedenle `StateMessage.aircraft` tek uçak değil, uçak ölçümlerinin listesidir.

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AircraftState {
    std::string id;
    double lat_deg;
    double lon_deg;
    double alt_msl_m;
    double yaw_deg;
    double pitch_deg;
    double roll_deg;
    double tas_mps;
};

struct StateMessage {
    std::uint64_t packet_no;
    std::uint64_t tick;
    double sim_time_s;
    std::vector<AircraftState> aircraft;
};

struct AircraftCommand {
    std::string id;
    double heading_deg;
    double altitude_m;
    double throttle;
};

struct CommandMessage {
    std::uint64_t based_on_tick;
    std::vector<AircraftCommand> aircraft;
};
```

### C++: cpp/include/PythonBridge.hpp

Bu sınıf C++ tarafından kullanılacak bağlantı kapısıdır. `simulation.read_state()` dediğinde içeride Python çağrılır; kontrol kodunun bu dönüşüm ayrıntısını bilmesi gerekmez.

`public` bölümündeki fonksiyonlar dışarıdan çağrılabilir. `private` bölümündeki alanlar sınıfın içinde kullanılır. `Impl` adı ‘iç uygulama’ için seçilmiş bir isimdir; pybind11 nesnelerini burada saklayarak Python ayrıntılarını `Controller` dosyalarından uzak tutarız. `std::unique_ptr` bu iç nesnenin sahipliğini tutar.

Kurucu fonksiyon `PythonBridge(...)` bağlantıyı açacak. `~PythonBridge()` ise kapanırken kayıtları ve Python nesnelerini uygun sırada serbest bırakacak. Bunların gövdeleri bu taslakta henüz yazılı değildir.

Python nesneleri başlık dosyasına ve Controller'a taşınmaz. `Impl` sınıfının içinde tutulur.

```cpp
#pragma once

#include <memory>
#include <string>
#include "messages.hpp"

class PythonBridge {
public:
    explicit PythonBridge(const std::string& scenario_path);
    ~PythonBridge();

    StateMessage read_state();
    CommandMessage initial_commands();
    void step(const CommandMessage& command);
    bool finished() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

### C++: cpp/src/PythonBridge.cpp dönüşüm örnekleri

Bu bölümün işi kontrol kararı vermek değil, iki dildeki veri kaplarını birbirine çevirmektir:

1. `read_state`: Python'daki ölçüm sözlüğünün alanlarını okuyup `StateMessage` içine kopyalar.
2. `apply_command`: C++ `CommandMessage` alanlarından bir Python sözlüğü oluşturur ve Python'un `step()` fonksiyonuna verir.

`py::object` bir Python nesnesini, `py::dict` Python sözlüğünü, `py::list` Python listesini temsil eder. `attr("read_state")()` yazımı ‘bu Python nesnesinin read_state fonksiyonunu çağır’ demektir. `cast<double>()` gibi ifadeler ise okunan değeri C++ veri tipine dönüştürür.

Aşağıdaki fonksiyonlar ilk sohbet örneğindeki serbest fonksiyonlardır. Son uygulamada `PythonBridge` yöntemlerinin içinde kullanılacaklar. Python yorumlayıcısı bunlar çağrılmadan önce başlatılmış olmalıdır.

```cpp
#include <pybind11/embed.h>
#include "messages.hpp"

namespace py = pybind11;

StateMessage read_state(py::object& simulation) {
    // Python ölçümü döndürür; sonuçtan alanları okuyacağız.
    auto data = simulation.attr("read_state")().cast<py::dict>();

    StateMessage state;
    state.packet_no = data["packet_no"].cast<std::uint64_t>();
    state.tick = data["tick"].cast<std::uint64_t>();
    state.sim_time_s = data["sim_time_s"].cast<double>();

    for (auto item : data["aircraft"].cast<py::list>()) {
        auto aircraft = py::cast<py::dict>(item);
        state.aircraft.push_back({
            aircraft["id"].cast<std::string>(),
            aircraft["lat_deg"].cast<double>(),
            aircraft["lon_deg"].cast<double>(),
            aircraft["alt_msl_m"].cast<double>(),
            aircraft["yaw_deg"].cast<double>(),
            aircraft["pitch_deg"].cast<double>(),
            aircraft["roll_deg"].cast<double>(),
            aircraft["tas_mps"].cast<double>()
        });
    }
    return state;
}

void apply_command(py::object& simulation, const CommandMessage& command) {
    py::dict message;
    message["based_on_tick"] = command.based_on_tick;
    py::list aircraft_commands;

    for (const auto& aircraft : command.aircraft) {
        py::dict item;
        item["id"] = aircraft.id;
        item["heading_deg"] = aircraft.heading_deg;
        item["altitude_m"] = aircraft.altitude_m;
        item["throttle"] = aircraft.throttle;
        aircraft_commands.append(item);
    }
    message["aircraft"] = aircraft_commands;
    // Burada doğrudan Python step(message) çağrısı yapılır.
    // Fonksiyon dönünce komutun uygulandığı adım tamamlanmıştır.
    simulation.attr("step")(message);
}
```

Başlatma kodunda `py::scoped_interpreter`, Python yorumlayıcısını C++ programının içinde başlatır. `sys.path` içine `src` eklemek, Python'un `sim_bridge.py` dosyasını bulmasını sağlar. `module_::import("sim_bridge")` o dosyayı modül olarak yükler; ardından `create_simulation()` çağrılır.

Başlangıç komutları bir kez okunur. Bunları her döngüde ölçülen heading ile yeniden oluşturmayız; örneğin YAML'da hedef 90 dereceyse uçak henüz 85 derecede olsa da hedef 90 olarak kalır.

Python'u başlatıp simülasyon nesnesini elde eden ilk örnek:

```cpp
py::scoped_interpreter python{};

// Workspace çalışma dizini varsayılır.
py::module_::import("sys").attr("path").attr("insert")(0, "src");
auto bridge = py::module_::import("sim_bridge");
auto simulation = bridge.attr("create_simulation")();

// Başlangıç komutlarını bir kez Python/YAML'dan al.
std::vector<AircraftCommand> targets;
auto initial = simulation.attr("initial_commands")().cast<py::list>();
for (auto item : initial) {
    auto aircraft = py::cast<py::dict>(item);
    targets.push_back({
        aircraft["id"].cast<std::string>(),
        aircraft["heading_deg"].cast<double>(),
        aircraft["altitude_m"].cast<double>(),
        aircraft["throttle"].cast<double>()
    });
}

// Doğrudan ateşleme çağrısı örneği; otomatik çağrılmayacak.
// bool launched = simulation.attr("fire")("blue", "red").cast<bool>();
```

Gömülü yorumlayıcı `.venv` ortamını kendiliğinden seçmiş kabul edilmemeli. Python yolu ve paketler uygulamada açıkça bağlanacak. `scenario_path` parametresinin YAML yükleyiciye iletilmesi de henüz örnekte yapılmadı. Python nesneleri yorumlayıcı kapatılmadan önce serbest bırakılmalı.

### C++: cpp/include/Controller.hpp

Controller, ölçümü alıp komutu üreten parçadır. Başlangıç hedeflerini kurucuda alır ve `targets_` içinde saklar. Bu alanın adındaki son alt çizgi, sınıfa ait saklanan bir değişken olduğunu göstermek için kullanılan isimlendirmedir; özel bir C++ işlemi değildir.

`calculate(const StateMessage& state)` ölçümü değiştirmeden kullanır. Buradaki `const` ‘ölçümü değiştirme’, `&` ise ‘nesnenin tamamının yeni bir kopyasını çıkarmadan kullan’ anlamındadır. Dönüş tipi `CommandMessage`, hesabın sonucunun komut nesnesi olduğunu söyler.

```cpp
#pragma once
#include "messages.hpp"

class Controller {
public:
    explicit Controller(const CommandMessage& initial_commands);
    CommandMessage calculate(const StateMessage& state);

private:
    CommandMessage targets_;
};
```

### C++: cpp/src/Controller.cpp

Kullanıcının değiştireceği hesaplama ve karar kodu burada bulunur. Örnek, yalnızca YAML'daki başlangıç yön/irtifa/gaz hedeflerini korur; taktik karar eklemez.

`CommandMessage command = targets_` saklanan hedeflerin bir kopyasını oluşturur. Böylece o döngünün komutunu hazırlayabiliriz. `based_on_tick = state.tick` komutun hangi ölçüme göre hesaplandığını yazar. `return command` hazırlanan komutu ana döngüye verir.

Önemli ayrım: Bu örnekte `command` içinde yaptığın değişiklikler yalnızca o döngünün komutunu değiştirir. Sonraki döngülerde de aynı yeni hedefi sürdürmek istiyorsan hedefini `targets_` içinde veya kendi kontrol durumunda saklamalısın.

Kendi algoritmanı yazarken `state.aircraft` listesinden kimliği `blue` ve `red` olan uçakları bulursun. Konum/hızlarını okur, hesabını yapar, `command.aircraft` içinde ilgili uçağın heading/irtifa/gaz alanlarını değiştirirsin. Bu dosyada Python sözlüğü veya pybind11 çağrısı yazman gerekmez.

```cpp
#include "Controller.hpp"

Controller::Controller(const CommandMessage& initial_commands)
    : targets_(initial_commands) {}

CommandMessage Controller::calculate(const StateMessage& state) {
    CommandMessage command = targets_;
    command.based_on_tick = state.tick;

    // Ölçümleri state üzerinden oku.
    // Kendi hesaplarını burada yap.
    // Uçuş komutlarını ve ateşleme isteklerini command içine yaz.

    return command;
}
```

### C++: cpp/src/main.cpp

Bu dosya başlangıç noktasıdır; asıl manevra hesaplarını burada biriktirmeyiz. Aşağıdaki dört işlem bütün akışı yönetir:

1. Bağlantıyı aç ve simülasyonu oluştur.
2. YAML'daki ilk hedeflerle Controller'ı oluştur.
3. Süre bitene kadar ölçümü oku, hesabı yap ve komutu uygula.
4. Döngüden çıkınca bağlantının kapanış işlemlerini yap.

`while (!simulation.finished())` ifadesi ‘simülasyon bitmediği sürece tekrarla’ demektir. Başındaki `!`, mantıksal olarak tersini alır. Her döngüde yeni `state` ve `command` nesneleri oluşturulur; uçakları her seferinde yeniden spawn etmeyiz.

```cpp
#include "PythonBridge.hpp"
#include "Controller.hpp"

int main() {
    // 1. Python bağlantısı ve simülasyon oturumu.
    PythonBridge simulation("scenario.yaml");

    // 2. Kullanıcının kontrol algoritması ve başlangıç hedefleri.
    Controller controller(simulation.initial_commands());

    while (!simulation.finished()) {
        // 3. Mevcut uçak durumlarını oku; fizik henüz ilerlemez.
        StateMessage state = simulation.read_state();

        // 4. Bu ölçüme göre yeni komutları hesapla.
        CommandMessage command = controller.calculate(state);

        // 5. Komutları uygula, fizik ilerlesin, adım tamamlanınca dön.
        simulation.step(command);
    }
}
```

### Derleme ve tamamlanacak noktalar

`CMakeLists.txt` henüz yazılmadı. C++ hedefi pybind11'in `pybind11::embed` hedefiyle bağlanacak; Python 3.11 ve mevcut `.venv` kullanılacak. Örnekler derleme/çalıştırma doğrulaması yapılmış kaynak dosyalar değildir.

Son uygulamada: import sırasında uzun uçuş döngüsünü kaldırma, YAML yükleyiciyi bağlama, `PythonBridge::Impl` ve kaynak yaşam döngüsünü tamamlama, birleşik JSON şablonlarını güncelleme, füze ölçümleri ve ateşleme sonuçlarını ekleme, komutları doğrulama ve recorder'ı kapatma tamamlanacak.


## Kendi kodunu yazmaya nereden başlayacaksın?

İlk hedef, hiçbir taktik karar vermeden YAML'daki yön ve irtifa hedefleriyle uçakları ilerletmek. Bağlantı çalıştıktan sonra `Controller.cpp` içindeki `calculate()` fonksiyonuna kendi hesabını ekleyeceksin. Böylece hem ölçüm hem komut aynı döngüde kalacak.

Bir sonraki aşamada füze ölçümleri `StateMessage` içine, ateşleme istekleri `CommandMessage` içine eklenecek. İstek, örneğin ‘blue, red'e ateş etsin’ şeklinde olacak. Python bu isteği bir kez uygulayıp sonucu bildirecek. Yukarıdaki ayrı `fire()` çağrısı yalnızca mekanizmayı anlatmak için bırakılmış bir örnektir; son akışta komut mesajının parçası olacak.

Recorder bu kararları vermez; oluşan durumu kaydeder. Arayüz de uçuş kararı vermez ve simülasyonu ayrıca ilerletmez; gelen veriyi gösterir. Başlangıç/spawn ayarlarını değiştirmek için YAML'a, uçuş kararlarını değiştirmek için `Controller.cpp` dosyasına bakacaksın.

Başka sohbette devam etmek için: ‘Bu dosyadaki doğrudan Python çağrısı ve dosyalara ayrılmış C++ mimarisiyle devam edelim; örnekler henüz uygulanmadı’ demen yeterlidir.
