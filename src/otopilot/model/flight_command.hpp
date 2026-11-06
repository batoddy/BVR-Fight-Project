#pragma once

#include <optional>

#include "common/angles.hpp"

// Gaz seviyeleri, 0..1 komut. F-16 modeli komutu ×2 uygular: 0.5 askeri güç, üstü art yakıcı.
// Manevralar gazı sayıyla değil bu adlarla yazar: command.set_throttle(throttle::MAX_THRUST)
namespace throttle
{
    inline constexpr double IDLE = 0.0;         // rölanti
    inline constexpr double DEFAULT = 0.45;     // seyir: askeri gücün biraz altı (scenario.yaml başlangıç gazı)
    inline constexpr double MRT = 0.5;          // askeri güç (Military Rated Thrust): art yakıcısız en yüksek
    inline constexpr double MAX_THRUST = 1.0;   // tam art yakıcı
}

// Python'un uçuş kontrolcüsüne giden hedef: değiştirilene kadar her step aynısı gider.
// Setter'lar değeri geçerli aralıkta tutar; geçersiz komut oluşturulamaz. Zincirlenebilir:
//   own.command.set_heading_deg(bearing).set_throttle(throttle::MAX_THRUST).set_g_limit(6.0, own.profile.max_g);
class FlightCommand
{
public:
    /// derece, herhangi bir açı; ±180'e sarılır
    FlightCommand& set_heading_deg(double deg)
    {
        heading_deg_ = wrap_180(deg);
        return *this;
    }

    /// ft, MSL
    FlightCommand& set_altitude_ft(double ft)
    {
        altitude_ft_ = ft;
        return *this;
    }

    /// 0..1; 0..0.5 rölantiden askeri güce, 0.5..1 art yakıcı. Aralık dışı en yakın sınıra çekilir.
    FlightCommand& set_throttle(double throttle)
    {
        if (throttle < 0.0)
        {
            throttle = 0.0;
        }
        if (throttle > 1.0)
        {
            throttle = 1.0;
        }
        throttle_ = throttle;
        return *this;
    }

    /// g, dönüşte çekilecek en büyük yük. @param max_g uçağın sınırı (profile.max_g); g bunu aşarsa max_g'ye çekilir.
    FlightCommand& set_g_limit(double g, double max_g)
    {
        if (g > max_g)
        {
            g = max_g;
        }
        g_limit_ = g;
        return *this;
    }

    /// g sınırını kaldır: Python kendi varsayılanını kullanır.
    FlightCommand& clear_g_limit()
    {
        g_limit_.reset();
        return *this;
    }

    // Okuyucular: setter'ların yazdığı değerler; g_limit yoksa Python varsayılanı.
    double heading_deg() const { return heading_deg_; }
    double altitude_ft() const { return altitude_ft_; }
    double throttle() const { return throttle_; }
    std::optional<double> g_limit() const { return g_limit_; }

private:
    double heading_deg_ = 0;          // derece ±180, 0 kuzey, + doğu | heading_deg (0..360, IO'da çevrilir)
    double altitude_ft_ = 0;          // ft                           | altitude_m (IO'da ft → m)
    double throttle_ = 0;             // 0..1                         | throttle
    std::optional<double> g_limit_;   // g; yoksa Python varsayılanı  | g_limit
};
