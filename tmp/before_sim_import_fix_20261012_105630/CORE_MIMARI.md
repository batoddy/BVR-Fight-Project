# Core mimari — Angajman Proje

Son güncelleme: **2026-10-12**. Bu dosya sohbetler arasında ortak mimari hafızadır.

## Temel kararlar

- **C++ çekirdek:** aynı uygulamada paketler/kütüphaneler; aralarında tipli yapılar ve fonksiyon çağrıları. Ayrı node, ROS, shared memory veya mesaj aracısı zorunlu değil.
- **Python yalnızca BVRGym tarafında:** ince köprü nesneleri oluşturur, dış komutları uygular, fiziği ilerletir, durum/olay verir. Taktik karar vermez; mevcut PID ve füze güdümü şimdilik içeride kalır.
- **Platform bağımsızlığı:** ilk adaptör JSBSim/BVRGym. Yeni platformlar aynı `StateSource` / `CommandSink` sözleşmelerine bağlanır; JSBSim formatını taklit etmez.
- **Başlatma:** kendi senaryo XML’imiz → isim, sabit ID, takım, model profili, spawn, mühimmat, ilk komut. JSBSim fizik/model XML’leri ayrı. Senaryo yükleyicisini biz yazacağız.
- **Veri:** SI birimleri; çekirdekte açılar radyan. Konum jeodezik WGS84, irtifa referansı açık; geometri ortak NED referansında. Zaman, reset dönemi ve geçerlilik korunur; eksik veri sıfır sayılmaz.
- **Komutlar:** `FlightSetpoint` = ID + heading/irtifa/gaz + geçerlilik. `LaunchMissile` = emir ID + atan/hedef ID. Tek etkin kontrol kaynağı; ateşleme tekrarları engellenir, kabul ve uygulanma ayrı bildirilir.
- **Mevcut prototip iletişimi:** ayrı Python/C++ süreçleri arasında tek TCP bağlantısında UTF-8 satır başına JSON (`kind`, `tick`, `data`). Python bütün uçak/füze ölçümünü gönderir, aynı tick için bütün uçakların heading/irtifa/gaz komutunu bekler, sonra ilerler. Python köprüsü uygulandı; C++ istemci henüz eklenmedi. Gömülü Python/pybind11 yerine bu yöntem seçildi. Canlı viewer yayını ve çekirdeğin ortak wire formatı sonraki aşamalardır.
- **Hesaplar:** hafif geometri ve ağır zarf ayrı. WEZ/MAR/DOR vb. model/profil gerektirir. Otonomi içeriği şimdilik kapsam dışı.
- **Görüntüleme/kayıt:** uçuş kodu UI bilmez. LiveSource ve ReplaySource aynı 2D/3D modeli besler; veri kesilince uçak süresiz ilerletilmez. ACMI + ortak state kaydı + hesap/plan/olay sidecarları. Tacview canlı çıkışı ayrı adaptör.

## Paketler ve akış

Hedef: `Workspace/cpp/packages/{common, platform_io, calculations, autonomy, planning, flight_control, recording, visualization_bridge}`.

```text
Senaryo XML → Python BVRGym köprüsü ↔ C++ platform_io
                                      ↓ durum     ↑ komut
                              hesaplar → plan/kontrol
                                      ↓
                               kayıt + canlı arayüz
```

## Uygulama durumu

**C++ çekirdeği plan aşamasında; Python TCP köprüsü uygulandı.** `Workspace/src/simulation.py` config kopyalama, ajan oluşturma, ölçüm ve ilerletme sınıfıdır. `connector.py` yalnızca JSON mesaj gönderir/alır. `sim_main.py` YAML, simülasyon, TCP ve recorder'ı birleştirir. `sim_local.py` aynı sınıfla C++ olmadan yerel uçuş yapar. Eski bvrgym_sim dosyası kullanıcının ayrı çalışmasıdır. Config'ler bağımsızdır; modül import'u uçuş veya sunucu başlatmaz. Yanlış/eksik komut fizik ilerlemeden reddedilir; kopmada kayıtlar kapanır. Viewer CSV replay yapar. YAML mevcut prototip, XML ilerideki hedef formatıdır. C++ istemci/çekirdeği, XML yükleyicisi ve canlı viewer adaptörü henüz uygulanmadı. Açılar bu prototip mesajında derece; çekirdekte radyan kararı gelecekteki adaptöre aittir.

`Workspace/src/recorder.py` bu değişiklikte korunmuştur: uçak ve füze başına ham CSV, hedef/fırlatma/isabet/kayıp/sağlık olaylarını `events.jsonl`, senaryo ve sütun kaynaklarını `manifest.json` içinde kaydeder. Atılmamış füzelerin dosyası yalnızca başlıktır; uçuş verisi atıştan sonra yazılır. Mevcut kaynak/birim ve koordinat bilgisi recorder'dadır. TCP durumunda jeodezik enlem, JSBSim `h-sl-meters` irtifası, derece yönelim ve m/s hız okunur; geoid dönüşümü yapılmaz. `Simulation.fire()` açık Python atış çağrısıdır; TCP komutunda füze ateşleme henüz bağlanmadı. Ayrıntılı kullanım: [Workspace/src/README.md](Workspace/src/README.md). TCP şablonları: [Workspace/docs/mimari/mesaj_sablonlari/tcp](Workspace/docs/mimari/mesaj_sablonlari/tcp/README.md).

Sıra: **tipler + XML → C++ komut çevrimi → canlı/kayıt → geometri → açık füze emri → rota/genişleme**.

## Güncelleme kuralı

Mimari karar veya uygulama durumu değişince bu dosyayı aynı iş kapsamında güncelle. Öneriyi uygulanmış gibi yazma; geçersiz kararları kaldır, tarihi yenile. Büyük değişiklikte [detaylı planı](Workspace/docs/mimari/BVR_SISTEM_MIMARISI.md) da uyumlu tut. Veri alanları: [simülasyon referansı](Workspace/docs/veri_info/SIMULASYON_VERI_REFERANSI.md).

