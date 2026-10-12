# Core mimari — Angajman Proje

Son güncelleme: **2026-10-10**. Bu dosya sohbetler arasında ortak mimari hafızadır.

## Temel kararlar

- **C++ çekirdek:** aynı uygulamada paketler/kütüphaneler; aralarında tipli yapılar ve fonksiyon çağrıları. Ayrı node, ROS, shared memory veya mesaj aracısı zorunlu değil.
- **Python yalnızca BVRGym tarafında:** ince köprü nesneleri oluşturur, dış komutları uygular, fiziği ilerletir, durum/olay verir. Taktik karar vermez; mevcut PID ve füze güdümü şimdilik içeride kalır.
- **Platform bağımsızlığı:** ilk adaptör JSBSim/BVRGym. Yeni platformlar aynı `StateSource` / `CommandSink` sözleşmelerine bağlanır; JSBSim formatını taklit etmez.
- **Başlatma:** kendi senaryo XML’imiz → isim, sabit ID, takım, model profili, spawn, mühimmat, ilk komut. JSBSim fizik/model XML’leri ayrı. Senaryo yükleyicisini biz yazacağız.
- **Veri:** SI birimleri; çekirdekte açılar radyan. Konum jeodezik WGS84, irtifa referansı açık; geometri ortak NED referansında. Zaman, reset dönemi ve geçerlilik korunur; eksik veri sıfır sayılmaz.
- **Komutlar:** `FlightSetpoint` = ID + heading/irtifa/gaz + geçerlilik. `LaunchMissile` = emir ID + atan/hedef ID. Tek etkin kontrol kaynağı; ateşleme tekrarları engellenir, kabul ve uygulanma ayrı bildirilir.
- **Dış iletişim önerisi:** UDP durum; çerçevelenmiş TCP komut/olay/snapshot. Wire şeması henüz kesinleşmedi. Native JSBSim UDP de adaptörde okunabilir; tek başına ajan/envanter bilgisi taşımaz. Nesne başına tek otoriter durum kaynağı.
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

**C++ mimarisi plan aşamasında.** Mevcut `Workspace/src/bvrgym_sim.py`, `scenario.yaml` üzerinden uçak oluşturup yerel Python komutlarıyla uçuruyor; viewer CSV replay yapıyor. YAML mevcut prototip, XML bu mimarinin hedef formatı. C++ çekirdeği, XML yükleyicisi ve canlı/dış komut köprüsü henüz uygulanmadı.

`Workspace/src/recorder.py` BVR için gereken ham alanları hesap yapmadan yazıyor: uçak başına 19, füze başına 17 sütun. `Latitude` jeosantriktir; jeodezik enlem ayrı sütunda saklanır ve WGS84 dönüşümü kayıtta yapılmaz. Uygulanan dış uçuş komutu (`command/*`) snapshot başına kaydedilir. `events.jsonl` hedef ataması, fırlatma, isabet, kayıp ve sağlık değişimini zaman damgalı tutar; `manifest.json` senaryo, sütun kaynakları, birimler, koordinat referansı ve fizik adımını bildirir. Planlanan rota, geometri ve zarf kayıtları henüz yok — bunlar ayrı dosyalar olarak sonradan eklenecek. Görüntüleyici manifest özetini oturum panelinde gösteriyor; olay işaretleri ve mühimmat sonucu ise Görünüm menüsündeki **Füze vuruş bilgisi** tercihine bağlı ve varsayılan kapalı. Sonuç hiçbir durumda ölçümden çıkarılmıyor. Mevcut durum analizi: [Workspace/docs/mevcut_durum/MEVCUT_VERI_AKISI.md](Workspace/docs/mevcut_durum/MEVCUT_VERI_AKISI.md).

Sıra: **tipler + XML → C++ komut çevrimi → canlı/kayıt → geometri → açık füze emri → rota/genişleme**.

## Güncelleme kuralı

Mimari karar veya uygulama durumu değişince bu dosyayı aynı iş kapsamında güncelle. Öneriyi uygulanmış gibi yazma; geçersiz kararları kaldır, tarihi yenile. Büyük değişiklikte [detaylı planı](Workspace/docs/mimari/BVR_SISTEM_MIMARISI.md) da uyumlu tut. Veri alanları: [simülasyon referansı](Workspace/docs/veri_info/SIMULASYON_VERI_REFERANSI.md).
