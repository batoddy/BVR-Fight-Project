#pragma once

#include <optional>

#include "common/angles.hpp"

// Python'un uçuş kontrolcüsüne giden hedef: değiştirilene kadar her step aynısı gider.
// Setter'lar değeri geçerli aralıkta tutar; geçersiz komut oluşturulamaz. Zincirlenebilir:
//   own.command.set_heading_deg(bearing).set_throttle(1.0).set_g_limit(6.0);
class FlightCommand
{
public:
    static constexpr double G_LIMIT_MIN = 1.5;   // g, Python'un kabul ettiği en küçük g sınırı
    static constexpr double G_LIMIT_MAX = 9.0;   // g, F-16 yapısal sınırı

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

    /// g, dönüşte çekilecek en büyük yük; aralık dışı G_LIMIT_MIN ya da G_LIMIT_MAX'a çekilir.
    FlightCommand& set_g_limit(double g)
    {
        if (g < G_LIMIT_MIN)
        {
            g = G_LIMIT_MIN;
        }
        if (g > G_LIMIT_MAX)
        {
            g = G_LIMIT_MAX;
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
