#include "model/aircraft.hpp"
#include <algorithm>
#include <cmath>

double Aircraft::get_turn_radius_mt() const
{
    // Hızı knot'tan m/s'ye çevir.
    const double speed_mps = knot_to_mps(kinematics.tas_kt);

    // Out manevrasında uygulanacağını varsaydığımız yük faktörü.
    const double n = profile.out_g;

    // Yatay ivme: g * sqrt(n² - 1)
    // Dönüş yarıçapı: hız² / yatay ivme
    const double radius_m = (speed_mps * speed_mps) /
        (GRAVITY_MPS2 * std::sqrt(n * n - 1.0));

    return radius_m;
}

double Aircraft::get_after_out_speed_kt() const
{
    return kinematics.tas_kt;
}

// Out dönüşü süresi: Δt = Δψ · r / V
//   Δψ: düşmanı kuyruğa almak için dönülecek açı = 180° - |ATA|  (radyan)
//   r:  dönüş yarıçapı (m)
//   V:  dönüş boyunca ortalama hız (m/s)
double Aircraft::get_out_time_spend_sec() const
{
    // ATA ±180; işaret yalnız dönüş yönünü söyler, açı için mutlak değer.
    const double turn_deg = 180.0 - std::fabs(enemy.ata_deg);
    const double turn_rad = DEG_TO_RAD * turn_deg;

    // Yay uzunluğu: açı · yarıçap
    const double arc_m = turn_rad * get_turn_radius_mt();

    // Dönüş başı ve sonu hızlarının ortalaması, m/s
    const double average_kt = (kinematics.tas_kt + get_after_out_speed_kt()) / 2.0;
    const double average_mps = knot_to_mps(average_kt);

    return arc_m / average_mps;
}

// Out dönüşü bitince düşmana yatay mesafedeki değişim, NM; + yaklaştık, - uzaklaştık.
// Düşman bakışında eksenler: x düşmana doğru, y yana. Dönüş yayının bitiş noktası:
//   x = -r · sin|ATA|
//   y =  r · (1 + cos|ATA|)
//   yeni mesafe = sqrt((R - x)² + y²)
//   yaklaşma    = R - yeni mesafe
double Aircraft::get_out_approach_nm() const
{
    const double radius_m = get_turn_radius_mt();
    const double ata_rad = DEG_TO_RAD * std::fabs(enemy.ata_deg);

    // Dönüş bitince düşmana göre konumumuz (dönüş başı orijin)
    const double x_m = -radius_m * std::sin(ata_rad);
    const double y_m = radius_m * (1.0 + std::cos(ata_rad));

    // Dönüş bitince düşmana yatay mesafe
    const double range_m = nmile_to_meter(enemy.range_2d_nm);
    const double along_m = range_m - x_m;
    const double new_range_m = std::sqrt(along_m * along_m + y_m * y_m);

    return meter_to_nmile(range_m - new_range_m);
}


bool Aircraft::fire_missile(const MissileProfile& type)
{
    // Emri verilmiş ama Python'un henüz uçurmadığı bir füzemiz varken yenisini verme:
    // iki emir aynı step'te giderse tek launch gider, eşleştirme kayar. Havadaki füze engel değil.
    int fired_count = 0;
    for (const Missile& m : missiles)
    {
        if (m.fired)
        {
            fired_count += 1;
            if (m.python_id.empty())
            {
                return false;   // emri verilmiş ama henüz uçmamış
            }
        }
    }

    for (Missile& m : missiles)
    {
        if (!m.fired && m.profile.name == type.name)
        {
            m.fired = true;
            m.fire_order = fired_count + 1;
            launch_order = true;
            return true;
        }
    }

    return false;   // o tipte atılmamış füze kalmadı
}

int Aircraft::missiles_left() const
{
    int count = 0;
    for (const Missile& m : missiles)
    {
        if (!m.fired)
        {
            count += 1;
        }
    }
    return count;
}

bool Aircraft::has_active_missile_against(const Aircraft& target) const
{
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight() && missile.target_id == target.id)
        {
            return true;
        }
    }

    return false;
}

const Missile* Aircraft::last_fired() const
{
    const Missile* last = nullptr;
    for (const Missile& m : missiles)
    {
        if (m.fired && (last == nullptr || m.fire_order > last->fire_order))
        {
            last = &m;
        }
    }
    return last;
}
