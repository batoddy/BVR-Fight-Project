#pragma once

#include <stdexcept>
#include <string>

#include "model/profiles/missile_profiles.hpp"

// Uçak tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Yeni özellik: alanı struct'a, değerini tabloya aynı sırada yaz.
struct AircraftProfile
{
    std::string type;                        // scenario.yaml type: tabloda aranan ad
    std::string name;                        // görünen ad

    MissileProfile main_weapon;              // WEZ, NEZ, pole'lar ve düşmanın bana karşı R_cold/MAR hesabı bununla (yükten bağımsız; yük hello'dan)

    double max_g = 0;                        // g, yapısal sınır: komuttaki g_limit bunu aşamaz (FlightCommand::set_g_limit)

    double radar_range_nm = 0;     // NM, algılama menzili
    double radar_gimbal_deg = 0;   // derece, yarım açı: burundan sağa VE sola tarama açısı (60 = ±60)

    double out_g = 0.0;                    // g, out dönüşünde kullanılan yük faktörü (out planı ve Out manevrası)
    double out_offset_nm = 0;              // NM, MAR'a eklenen out payı
    double dr_notch_time_s = 15.0;         // s, DR hesabında notch bekleme süresi
    double recommit_fire_ata_deg = 30.0;   // derece, recommit'te atışın yapıldığı ATA; MRR hesabı bununla yapılır
};

// Değerlerin kaynağı:
//   F16  max_g, radar: açık kaynak (F-16C Block 50/52, AN/APG-68).
//   Su57, CustomAircraft: tamamen örnek. Fizik modeli her tipte F-16.
//   Out g'si ve payı: yaklaşık simülasyon ayarları; ölçülmüş performans verisi değil. Menziller NM'dir.
//                                        type              name               main_weapon  g     radar  gimbal  out_g  out_offset_nm
inline const AircraftProfile F16             = {"F16",            "F-16",            AIM120C,     9.0,  200,    60,     6.0,   0.5};
inline const AircraftProfile SU57            = {"Su57",           "Su-57",           AIM120C,     9.0,  200,    60,     6.0,   0.5};
inline const AircraftProfile CUSTOM_AIRCRAFT = {"CustomAircraft", "Ornek ozel ucak", AIM7M,       4.0,  200,    45,     6.0,   0.5};

inline const AircraftProfile& aircraft_profile(const std::string& type)   // hello'daki type → profil; tabloda yoksa hata
{
    static const AircraftProfile* const table[] = {&F16, &SU57, &CUSTOM_AIRCRAFT};
    for (const AircraftProfile* profile : table)
    {
        if (profile->type == type)
        {
            return *profile;
        }
    }
    throw std::runtime_error("Bilinmeyen ucak tipi: " + type + " (aircraft_profiles.hpp'ye ekle)");
}
