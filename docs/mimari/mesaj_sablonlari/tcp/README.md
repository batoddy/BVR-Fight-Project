# Python–C++ TCP mesaj örnekleri (protokol v2.1)

Akış: `hello` bir kez, sonra her dış adımda bir `state` (her uçağın kendi paketi) ve bir `command` (bütün uçuş komutları ve atışlar), en sonda `end`. Şema ve kurallar [MESAJ_PROTOKOLU_V2.md](../../MESAJ_PROTOKOLU_V2.md) belgesindedir. JSON dosyaları çalışma zamanında okunmaz; sözleşmenin somut örnekleridir.

Bu örnekler `src/simulator` (`Simulation`) ve C++ otopilotun (`src/otopilot/main.exe`) kısa bir gerçek koşusundan alınmıştır (40 NM, 15 s). `command`, koşudaki ilk atış emrinin adımıdır; `state` onun hemen ardından gelen ölçümdür: atanın paketinde `launch_accepted`, hedefin paketinde `missile_launch_warning` ve `warnings` vardır. Girintiler okunabilirlik içindir; ağda her mesaj tek satır JSON ve sonlandırıcı `\n` ile gönderilir.

Mesajda yalnız SI ve derece kullanılır (`_m`, `_mps`, `_deg`, `_s`, `_kg`); C++ kendi ft/NM/kt modeline giriş ve çıkışta dönüştürür. `calculations` alanları C++ hesaplarıdır ve NM/kt adlarıyla gider; `phase` manevranın adıdır.

| Dosya | İçerik |
|---|---|
| [hello.json](hello.json) | Protokol sürümü, zamanlama, uçaklar ve slot → füze profili / `designation` |
| [command.json](command.json) | İki uçuş hedefi (`mode`: `target`; `direct` modda `bank_deg`, `g`, `throttle`), atış emri (`id`, `target_id`; slotu sim seçer) ve C++ hesapları |
| [state.json](state.json) | Saat ve her uçağın paketi: `own`, `rwr`, `weapons`, `contacts`, `warnings`, `events`. Sensörler kapalı (`world.sensors: false`): `contacts` gerçek (`source: "truth"`), `rwr` boş. C++ `contacts` ve `rwr`'ı okumaz; düşmanı gerçek kinematiğiyle hesaplar |
| [end.json](end.json) | Süre bitişi, son tick ve boş veri nesnesi |

Örneklerdeki konum, ad, profil, süre ve mühimmat sayısı birer örnek değerdir. Hazır slotta `kinematics`, `flight_time_s`, `target_id`, `request_id` `null`'dur.

Protokolün kaynakları Python [connector.py](../../../../src/simulator/connector.py), [simulation.py](../../../../src/simulator/simulation.py), C++ [messages.hpp](../../../../src/otopilot/io/messages.hpp), [state_reader.cpp](../../../../src/otopilot/io/state_reader.cpp), [command_writer.cpp](../../../../src/otopilot/io/command_writer.cpp) ve [calculation_writer.cpp](../../../../src/otopilot/io/calculation_writer.cpp) dosyalarıdır.
