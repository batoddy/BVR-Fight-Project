# Python–C++ TCP mesaj örnekleri

Bu örnekler mevcut `Simulation`, `Sim` ve yazıcılar kullanılarak kısa bir gerçek koşudan alınmıştır. State ve command aynı dış adıma aittir; end dosyası aynı koşunun kapanışını gösterir. Girintiler okunabilirlik içindir; ağda her mesaj tek satır JSON ve sonlandırıcı `\n` ile gönderilir.

Birim geçişinde command örneğinin hesap alanları NM/kt adlarına ve sayısal
karşılıklarına dönüştürülmüştür. State ve uçuş komutları metre/m-s olarak kalır;
C++ kendi ft/NM/kt modeline giriş ve çıkışta dönüşüm uygular.

| Dosya | İçerik |
|---|---|
| [state.json](state.json) | Uçaklar, mühimmat, fizik saati ve son uygulanan komutlar |
| [command.json](command.json) | Aynı tick için iki uçuş hedefi, atış listesi ve C++ hesapları |
| [end.json](end.json) | Süre bitişi, son tick ve boş veri nesnesi |

Örneklerdeki konum, ad, profil, süre ve mühimmat sayısı birer örnek değerdir. Kendi koşunda YAML ve fizik sonucuna göre değişir. İlk ölçüm zamanının sıfır olduğu varsayılmaz. Hazır mühimmatın fizik alanları `null`, atılanın hedef kimliği ve kinematiği doludur.

Protokolün kaynakları Python [connector.py](../../../../src/sim/connector.py), [simulation.py](../../../../src/sim/simulation.py), C++ [messages.hpp](../../../../src/otopilot/io/messages.hpp), [command_writer.cpp](../../../../src/otopilot/io/command_writer.cpp) ve [calculation_writer.cpp](../../../../src/otopilot/io/calculation_writer.cpp) dosyalarıdır. Ayrıntılı alan sözleşmesi [veri referansındadır](../../../veri_info/SIMULASYON_VERI_REFERANSI.md).
