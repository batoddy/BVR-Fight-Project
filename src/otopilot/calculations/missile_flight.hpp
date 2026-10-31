#pragma once

#include <optional>

#include "model/missile_profiles.hpp"

// Füzenin uçuşu: atıldıktan sonra ne zaman nerede, hangi hızda. Saf fonksiyonlar.
// Model ve geçerlilik: model/missile_glide.hpp, calculations/fuze_ucus_tablosu.md
//
// "glide_" öneki: sonuçlar füzenin ÖLÇÜLMÜŞ uçuş eğrisinden (boost + süzülme) okunur.
//   - Yol ve süre ölçümün kendisidir (kayıttan integrali alınmış); ara değerle okunur.
//   - Hız fonksiyonları o noktadaki ANLIK hızı verir. Yol ya da süre bulmak için bu hızları
//     adım adım toplamayın (entegre etmeyin): glide_distance_at_time_nm / glide_time_at_distance_s
//     aynı sonucu zaten ölçülmüş haliyle verir.
//
// Ortak parametreler:
//   missile:            atılan füze (profile.glide modeli kullanılır)
//   launch_altitude_ft: ft, atış irtifası (modelin aralığı 4-10 km; dışında sınıra çekilir)
//   launch_speed_kt:    kt, atan uçağın atış anındaki hava hızı
// Sonuç yoksa (std::nullopt): füze o noktaya Mach 1'in altına düşüp kaybolmadan varamıyor ya da modeli yok.
// Yol füzenin aldığı yoldur, düz kabul edilir; güdümün dönen hedefe karşı kaybettirdiği yol yoktur.
namespace missile_flight
{
    /// s, füze distance_nm yolu atıştan kaç saniye sonra alır
    std::optional<double> glide_time_at_distance_s(const MissileProfile& missile, double launch_altitude_ft,
                                                   double launch_speed_kt, double distance_nm);

    /// kt, füze distance_nm yol aldığında anlık hızı
    std::optional<double> glide_speed_at_distance_kt(const MissileProfile& missile, double launch_altitude_ft,
                                                     double launch_speed_kt, double distance_nm);

    /// NM, füzenin atıştan time_s saniye sonra aldığı yol
    std::optional<double> glide_distance_at_time_nm(const MissileProfile& missile, double launch_altitude_ft,
                                                    double launch_speed_kt, double time_s);

    /// kt, füzenin atıştan time_s saniye sonraki anlık hızı
    std::optional<double> glide_speed_at_time_kt(const MissileProfile& missile, double launch_altitude_ft,
                                                 double launch_speed_kt, double time_s);

    /// NM, füzenin Mach 1'e düşüp kaybolana kadar alabileceği en uzun yol
    std::optional<double> glide_max_distance_nm(const MissileProfile& missile, double launch_altitude_ft,
                                                double launch_speed_kt);

    /// s, atış anında range_nm uzaktaki hedefi füze kaç saniyede yakalar.
    ///   target_closure_kt: hedefin atış noktasına doğru hızı; + bana geliyor, − benden kaçıyor (sabit kabul)
    ///   Yakalama: füzenin aldığı yol = range − hedefin o sürede yaklaştığı yol
    std::optional<double> glide_time_to_hit_s(const MissileProfile& missile, double launch_altitude_ft,
                                              double launch_speed_kt, double range_nm, double target_closure_kt);
}
