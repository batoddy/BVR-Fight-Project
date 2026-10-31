#include "calculations/missile_flight.hpp"

#include <cmath>
#include <vector>

#include "common/units.hpp"

namespace
{
    // Füzenin uçuşundan bir an: atıştan bu yana süre ve yol, o anki hız.
    struct FlightPoint
    {
        double time_s = 0;
        double distance_m = 0;
        double speed_mps = 0;
    };

    // m/s, ses hızı. Standart atmosfer, 11 km altı:
    //   sıcaklık T = 288.15 − 0.0065 · h   (K, h metre)
    //   ses hızı a = √(γ · R · T),  γ = 1.4, R = 287.05 J/(kg·K)
    double speed_of_sound_mps(double altitude_m)
    {
        const double temperature_k = 288.15 - 0.0065 * altitude_m;
        return std::sqrt(1.4 * 287.05 * temperature_k);
    }

    // a ile b arasında, fraction (0..1) kadar ilerideki değer
    double between(double a, double b, double fraction)
    {
        return a + (b - a) * fraction;
    }

    // Füzenin atıştan Mach 1'e düşene kadarki uçuşu, sırayla noktalar. Model: model/missile_glide.hpp
    std::vector<FlightPoint> flight_points(const GlideModel& glide, double launch_altitude_ft, double launch_speed_kt)
    {
        std::vector<FlightPoint> points;
        if (glide.reference == nullptr)
        {
            return points;   // bu füzenin modeli yok
        }
        const std::vector<GlidePoint>& reference = *glide.reference;

        // İrtifa, ölçülen aralığa çekilir.
        double altitude_km = feet_to_meter(launch_altitude_ft) / 1000.0;
        if (altitude_km < glide.min_altitude_km)
        {
            altitude_km = glide.min_altitude_km;
        }
        if (altitude_km > glide.max_altitude_km)
        {
            altitude_km = glide.max_altitude_km;
        }
        const double x = altitude_km - glide.reference_altitude_km;   // km

        // Ölçek: aynı Mach aralığında yol k katı. Süre = yol / hız; aynı Mach'ta hız ses hızıyla orantılı.
        //   yol  = k · referans yol
        //   süre = k · (a_referans / a) · referans süre
        const double sound_mps = speed_of_sound_mps(altitude_km * 1000.0);
        const double reference_sound_mps = speed_of_sound_mps(glide.reference_altitude_km * 1000.0);
        const double k = std::exp(glide.altitude_coefficient_per_km * x);
        const double time_scale = k * reference_sound_mps / sound_mps;

        // 1) Boost: atış hızından tepe hıza, sabit ivmeyle. Yol = ortalama hız × süre.
        const double launch_mps = knot_to_mps(launch_speed_kt);
        const double delta_v_mps = glide.delta_v_c0_mps + glide.delta_v_c1_mps_per_km * x + glide.delta_v_c2_mps_per_km2 * x * x;
        const double peak_mps = launch_mps + delta_v_mps;
        const double boost_m = (launch_mps + peak_mps) / 2.0 * glide.boost_time_s;

        points.push_back({0.0, 0.0, launch_mps});
        points.push_back({glide.boost_time_s, boost_m, peak_mps});

        // 2) Süzülme: referans eğriye tepe hızın Mach'ından girilir.
        //    Referansta aynı Mach'ın hızı = Mach × referans ses hızı. Eğrinin tepesinden hızlıysa tepeden girilir.
        const double peak_mach = peak_mps / sound_mps;
        GlidePoint entry = reference.front();
        for (size_t i = 1; i < reference.size(); ++i)
        {
            const GlidePoint& faster = reference[i - 1];
            const GlidePoint& slower = reference[i];
            const double entry_speed_mps = peak_mach * reference_sound_mps;
            if (entry_speed_mps <= faster.speed_mps && entry_speed_mps >= slower.speed_mps)
            {
                const double fraction = (faster.speed_mps - entry_speed_mps) / (faster.speed_mps - slower.speed_mps);
                entry.speed_mps = entry_speed_mps;
                entry.distance_m = between(faster.distance_m, slower.distance_m, fraction);
                entry.time_s = between(faster.time_s, slower.time_s, fraction);
                break;
            }
        }

        // Referans eğri giriş noktasından itibaren yürünür; her nokta bu irtifaya ölçeklenir.
        GlidePoint previous = entry;
        for (const GlidePoint& point : reference)
        {
            if (point.speed_mps >= entry.speed_mps)
            {
                continue;   // giriş noktasından hızlı: füze buraya hiç girmedi
            }

            // 3) Bitiş: Mach 1'in altına inince kaybolur. Son nokta tam Mach 1'de.
            const double mach = point.speed_mps / reference_sound_mps;
            GlidePoint scaled_from = point;
            const bool below_mach_1 = mach < 1.0;
            if (below_mach_1)
            {
                const double mach_1_mps = reference_sound_mps;
                const double fraction = (previous.speed_mps - mach_1_mps) / (previous.speed_mps - point.speed_mps);
                scaled_from.speed_mps = mach_1_mps;
                scaled_from.distance_m = between(previous.distance_m, point.distance_m, fraction);
                scaled_from.time_s = between(previous.time_s, point.time_s, fraction);
            }

            FlightPoint flight;
            flight.time_s = glide.boost_time_s + time_scale * (scaled_from.time_s - entry.time_s);
            flight.distance_m = boost_m + k * (scaled_from.distance_m - entry.distance_m);
            flight.speed_mps = scaled_from.speed_mps / reference_sound_mps * sound_mps;   // aynı Mach, bu irtifada
            points.push_back(flight);

            if (below_mach_1)
            {
                break;
            }
            previous = point;
        }
        return points;
    }

    // Uçuşta yolu distance_m olan an; füze oraya varamıyorsa yok.
    std::optional<FlightPoint> point_at_distance(const std::vector<FlightPoint>& points, double distance_m)
    {
        for (size_t i = 1; i < points.size(); ++i)
        {
            const FlightPoint& before = points[i - 1];
            const FlightPoint& after = points[i];
            if (distance_m >= before.distance_m && distance_m <= after.distance_m)
            {
                const double fraction = (distance_m - before.distance_m) / (after.distance_m - before.distance_m);
                return FlightPoint{between(before.time_s, after.time_s, fraction), distance_m,
                                   between(before.speed_mps, after.speed_mps, fraction)};
            }
        }
        return std::nullopt;
    }

    // Uçuşta atıştan time_s sonraki an; füze o ana kadar kaybolduysa yok.
    std::optional<FlightPoint> point_at_time(const std::vector<FlightPoint>& points, double time_s)
    {
        for (size_t i = 1; i < points.size(); ++i)
        {
            const FlightPoint& before = points[i - 1];
            const FlightPoint& after = points[i];
            if (time_s >= before.time_s && time_s <= after.time_s)
            {
                const double fraction = (time_s - before.time_s) / (after.time_s - before.time_s);
                return FlightPoint{time_s, between(before.distance_m, after.distance_m, fraction),
                                   between(before.speed_mps, after.speed_mps, fraction)};
            }
        }
        return std::nullopt;
    }
}

std::optional<double> missile_flight::glide_time_at_distance_s(const MissileProfile& missile, double launch_altitude_ft,
                                                               double launch_speed_kt, double distance_nm)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    const auto point = point_at_distance(points, nmile_to_meter(distance_nm));
    if (!point.has_value())
    {
        return std::nullopt;
    }
    return point->time_s;
}

std::optional<double> missile_flight::glide_speed_at_distance_kt(const MissileProfile& missile, double launch_altitude_ft,
                                                                 double launch_speed_kt, double distance_nm)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    const auto point = point_at_distance(points, nmile_to_meter(distance_nm));
    if (!point.has_value())
    {
        return std::nullopt;
    }
    return mps_to_knot(point->speed_mps);
}

std::optional<double> missile_flight::glide_distance_at_time_nm(const MissileProfile& missile, double launch_altitude_ft,
                                                                double launch_speed_kt, double time_s)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    const auto point = point_at_time(points, time_s);
    if (!point.has_value())
    {
        return std::nullopt;
    }
    return meter_to_nmile(point->distance_m);
}

std::optional<double> missile_flight::glide_speed_at_time_kt(const MissileProfile& missile, double launch_altitude_ft,
                                                             double launch_speed_kt, double time_s)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    const auto point = point_at_time(points, time_s);
    if (!point.has_value())
    {
        return std::nullopt;
    }
    return mps_to_knot(point->speed_mps);
}

std::optional<double> missile_flight::glide_max_distance_nm(const MissileProfile& missile, double launch_altitude_ft,
                                                            double launch_speed_kt)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    if (points.empty())
    {
        return std::nullopt;
    }
    return meter_to_nmile(points.back().distance_m);
}

// Füze ile hedef arasında kalan mesafe, atıştan t saniye sonra:
//   kalan = range − hedefin yaklaştığı yol − füzenin aldığı yol
//         = range − closure · t − füze_yolu(t)
// Kalan mesafenin sıfıra indiği an yakalama anıdır. Uçuş noktaları sırayla taranır;
// kalanın işaret değiştirdiği iki nokta arasında doğrusal ara değer alınır.
std::optional<double> missile_flight::glide_time_to_hit_s(const MissileProfile& missile, double launch_altitude_ft,
                                                          double launch_speed_kt, double range_nm, double target_closure_kt)
{
    const auto points = flight_points(missile.glide, launch_altitude_ft, launch_speed_kt);
    const double range_m = nmile_to_meter(range_nm);
    const double target_closure_mps = knot_to_mps(target_closure_kt);
    if (points.empty())
    {
        return std::nullopt;   // modeli yok
    }
    if (range_m <= 0.0)
    {
        return 0.0;            // hedef zaten füzenin üstünde
    }

    double previous_gap_m = range_m;   // atış anında kalan mesafe
    double previous_time_s = 0.0;
    for (const FlightPoint& point : points)
    {
        const double gap_m = range_m - target_closure_mps * point.time_s - point.distance_m;
        if (gap_m <= 0.0)
        {
            const double fraction = previous_gap_m / (previous_gap_m - gap_m);
            return between(previous_time_s, point.time_s, fraction);
        }
        previous_gap_m = gap_m;
        previous_time_s = point.time_s;
    }
    return std::nullopt;   // füze Mach 1'e düşene kadar yetişemedi
}
