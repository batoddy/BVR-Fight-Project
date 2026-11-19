#pragma once

#include <optional>

#include "common/angles.hpp"

// Gaz seviyeleri, 0..1 komut. F-16 modeli komutu ×2 uygular: 0.5 askeri güç, üstü art yakıcı.
// Manevralar gazı sayıyla değil bu adlarla yazar: command.set_throttle(throttle::MAX_THRUST)
namespace throttle
{
    inline constexpr double IDLE = 0.1;         // rölanti
    inline constexpr double CRANK = 0.35;         // rölanti
    inline constexpr double DEFAULT = 0.45;     // seyir: askeri gücün biraz altı (scenario.yaml başlangıç gazı)
    inline constexpr double MRT = 0.5;          // askeri güç (Military Rated Thrust): art yakıcısız en yüksek
    inline constexpr double MAX_THRUST = 1.0;   // tam art yakıcı
}

// Komut modu (protokol v2.1): Target yön/irtifa hedefi, Direct yatış + g. Henüz hiçbir manevra Direct kullanmaz.
enum class CommandMode
{
    Target,   // heading_deg, altitude_m, throttle, g_limit?
    Direct,   // bank_deg, g, throttle
};

// Doğrudan mod g sınırları: f16.xml FBW +9 / −4 g.
inline constexpr double DIRECT_G_MIN = -4.0;
inline constexpr double DIRECT_G_MAX = 9.0;

// Python'un uçuş kontrolcüsüne giden hedef: değiştirilene kadar her step aynısı gider.
// Setter'lar değeri geçerli aralıkta tutar; geçersiz komut oluşturulamaz. Zincirlenebilir:
//   own.command.set_heading_deg(bearing).set_throttle(throttle::MAX_THRUST).set_g_limit(6.0, own.profile.max_g);
class FlightCommand
{
public:
    FlightCommand& set_heading_deg(double deg)          // derece, herhangi bir açı; ±180'e sarılır. Target moda geçer
    {
        heading_deg_ = wrap_180(deg);
        mode_ = CommandMode::Target;
        return *this;
    }

    FlightCommand& set_altitude_ft(double ft)           // ft, MSL. Target moda geçer
    {
        altitude_ft_ = ft;
        mode_ = CommandMode::Target;
        return *this;
    }

    FlightCommand& set_bank_deg(double deg)             // derece, + sağ; ±180'e sarılır. Direct moda geçer
    {
        bank_deg_ = wrap_180(deg);
        mode_ = CommandMode::Direct;
        return *this;
    }

    FlightCommand& set_g(double g)                      // g, istenen yük; DIRECT_G_MIN..MAX dışı en yakın sınıra çekilir. Direct moda geçer
    {
        if (g < DIRECT_G_MIN)
        {
            g = DIRECT_G_MIN;
        }
        if (g > DIRECT_G_MAX)
        {
            g = DIRECT_G_MAX;
        }
        g_ = g;
        mode_ = CommandMode::Direct;
        return *this;
    }

    FlightCommand& set_throttle(double throttle)        // 0..1 (throttle:: seviyeleri); aralık dışı en yakın sınıra çekilir
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

    FlightCommand& set_g_limit(double g, double max_g)  // g, dönüşte en büyük yük; max_g (profile.max_g) aşılırsa ona çekilir
    {
        if (g > max_g)
        {
            g = max_g;
        }
        g_limit_ = g;
        return *this;
    }

    FlightCommand& clear_g_limit()                      // g sınırını kaldır: Python kendi varsayılanını kullanır
    {
        g_limit_.reset();
        return *this;
    }

    CommandMode mode() const { return mode_; }
    double heading_deg() const { return heading_deg_; }
    double altitude_ft() const { return altitude_ft_; }
    double throttle() const { return throttle_; }
    std::optional<double> g_limit() const { return g_limit_; }   // yoksa Python varsayılanı
    double bank_deg() const { return bank_deg_; }                // Direct modda
    double g() const { return g_; }                              // Direct modda

private:
    double heading_deg_ = 0;          // derece ±180, 0 kuzey, + doğu | heading_deg (0..360, IO'da çevrilir)
    double altitude_ft_ = 0;          // ft                           | altitude_m (IO'da ft → m)
    double throttle_ = 0;             // 0..1                         | throttle
    std::optional<double> g_limit_;   // g; yoksa Python varsayılanı  | g_limit
    CommandMode mode_ = CommandMode::Target;   //                       | mode ("target" / "direct")
    double bank_deg_ = 0;             // derece ±180, + sağ               | bank_deg (Direct)
    double g_ = 1;                    // g, DIRECT_G_MIN..MAX             | g (Direct)
};
