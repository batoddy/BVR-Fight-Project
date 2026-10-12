# Uygulanan TCP mesajları

12 Ekim 2026. Python'daki `src/connector.py` içindeki `Message` ile eşleşir.
Bu klasördeki JSON dosyaları şablondur; değerler temsilidir, ölçüm kaydı değildir.

- `state.json`: Bütün uçakların ve mühimmatın aynı adımdaki ölçümü.
- `command.json`: C++'ın aynı tick için bütün uçaklara verdiği heading/irtifa/gaz hedefleri ve o turun tek seferlik atış emirleri (`launch`, boş liste olabilir). Teldeki heading 0..360'tır; C++ içinde ±180 kullanılır, çeviri `src/otopilot/objects.cpp` içindedir.
- `end.json`: Simülasyon süresi tamamlandı; istemci döngüden çıkabilir.

Aktarımda JSON girintisiz tek satır yazılır ve sonuna `\n` eklenir. TCP portu
YAML'daki `connection` bölümündedir. Header yalnızca `kind`, `tick`, `data` içerir.
Henüz atılmamış füzenin fizik alanları `null`; atılınca `kinematics` uçakla
aynı konum/yönelim/hız alanlarını taşır. Füze sonuçları `hit`, `lost` veya `null`.
`reserved` davranışı uygulanmadı; bu sürümde böyle bir alan yayınlanmaz.

Üst klasördeki uçak başına v2 şablonları eski tasarım referansıdır. Mevcut TCP
uygulaması bu klasördeki birleşik mesajı kullanır. Ayrıntılı kullanım: `src/README.md`.
