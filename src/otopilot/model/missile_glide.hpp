#pragma once

#include <vector>

// Füzenin ölçülmüş uçuş eğrisinin bir noktası. Eğri tepe hızdan (boost bitişi) başlar.
//   distance_m ve time_s, tepe hızdan bu noktaya kadar ÖLÇÜLEN toplam yol ve süredir (kayıttan, integrali alınmış).
//   speed_mps o noktadaki anlık hızdır.
struct GlidePoint
{
    double speed_mps = 0;    // m/s, anlık hız
    double distance_m = 0;   // m, tepe hızdan beri alınan yol
    double time_s = 0;       // s, tepe hızdan beri geçen süre
};

// Füzenin uçuş modeli: tek bir ölçülmüş eğri (referans irtifada) + irtifa ve atış hızı düzeltmesi.
// Hesabı: calculations/missile_flight.cpp, nasıl bulunduğu: calculations/fuze_ucus_tablosu.md
//
//   1) Boost:  füze boost_time_s içinde atış hızından tepe hıza çıkar (sabit ivme kabulü).
//              tepe hız = atış hızı + Δv(h),   Δv(h) = c0 + c1·x + c2·x²,   x = h − referans irtifa (km)
//   2) Süzülme: füze referans eğriyi izler; eğriye tepe hızının Mach'ından girer.
//              Aynı Mach aralığında yol, referanstakinin k(h) katıdır:  k(h) = e^(b·x)
//              (Füzeyi yavaşlatan sürükleme hava yoğunluğuyla orantılı; yoğunluk irtifayla üstel düşer.)
//   3) Bitiş:  Mach 1'in altına inen füze kaybolur (BVRGym target_lost_below_mach = 1).
struct GlideModel
{
    const std::vector<GlidePoint>* reference = nullptr;   // referans eğri; nullptr ise bu füzenin modeli yok
    double reference_altitude_km = 10;                     // km, referans eğrinin ölçüldüğü irtifa
    double altitude_coefficient_per_km = 0;                // 1/km, b: k(h) = e^(b·x)
    double delta_v_c0_mps = 0;                             // m/s, Δv'nin referans irtifadaki değeri
    double delta_v_c1_mps_per_km = 0;                      // m/s/km
    double delta_v_c2_mps_per_km2 = 0;                     // m/s/km²
    double boost_time_s = 0;                               // s, atıştan tepe hıza kadar
    double min_altitude_km = 0;                            // km, ölçülen aralık; dışında sınıra çekilir
    double max_altitude_km = 0;                            // km
};

// AIM-120C (BVRGym) 10 km süzülme eğrisi, 25 m/s aralıklarla.
// Kaynak: docs/fuze_analiz/sonuclar.txt, "Birleşik hız eksenli tablolar", 10000 m
//   (mar_test_10km_..._090_throttle 1447→882 m/s + mar_test_10000km_..._055mah 882→122 m/s)
//                                                      hız m/s  yol m   süre s
inline const std::vector<GlidePoint> AIM120C_GLIDE_10KM = {
    {1447,      0,  0.0},   // tepe hız
    {1400,   1052,  0.7},
    {1375,   1627,  1.2},
    {1350,   2212,  1.6},
    {1325,   2808,  2.0},
    {1300,   3416,  2.5},
    {1275,   4034,  3.0},
    {1250,   4666,  3.5},
    {1225,   5310,  4.0},
    {1200,   5969,  4.5},
    {1175,   6646,  5.1},
    {1150,   7348,  5.7},
    {1125,   8076,  6.4},
    {1100,   8830,  7.0},
    {1075,   9611,  7.7},
    {1050,  10419,  8.5},
    {1025,  11257,  9.3},
    {1000,  12125, 10.2},
    { 975,  13026, 11.1},
    { 950,  13960, 12.1},
    { 925,  14928, 13.1},
    { 900,  15933, 14.2},
    { 875,  16981, 15.4},
    { 850,  18077, 16.6},
    { 825,  19208, 18.0},
    { 800,  20377, 19.4},
    { 775,  21580, 21.0},
    { 750,  22815, 22.6},
    { 725,  24090, 24.3},
    { 700,  25443, 26.2},
    { 675,  26860, 28.3},
    { 650,  28339, 30.5},
    { 625,  29929, 33.0},
    { 600,  31656, 35.8},
    { 575,  33534, 39.0},
    { 550,  35568, 42.6},
    { 525,  37769, 46.7},
    { 500,  40154, 51.4},
    { 475,  42751, 56.7},
    { 450,  45587, 62.9},
    { 425,  48698, 70.0},
    { 400,  52128, 78.3},
    { 375,  54411, 84.1},
    { 350,  55191, 86.3},
    { 325,  55691, 87.8},
    { 300,  56078, 89.0},
    { 275,  56477, 90.4},   // Mach 1 (10 km'de 299.5 m/s) bu iki satır arasında
};

// AIM-120C modeli. Katsayılar 4-10 km, atış 290-560 m/s, 21 mar_test kaydından:
//   b = 0.11:  eğriler Mach ekseninde karşılaştırıldı; yol hatası RMS %2.3
//   Δv:        sonuclar.txt "Δv(h) fit'i", en büyük artık 3.4 m/s
//   boost:     21 kayıtta boost yolu / ortalama hız = 0.363-0.375 s
inline const GlideModel AIM120C_GLIDE = {&AIM120C_GLIDE_10KM, 10.0, 0.11, 889.3, 19.48, -1.532, 0.37, 4.0, 10.0};
