#pragma once

#include <map>
#include <optional>
#include <string>

struct AircraftProfile {
    std::string name;
    std::optional<double> max_mach;  // Boşsa profil değeri henüz girilmedi.
    std::optional<double> max_g;
};

// Anahtar: Python'dan gelen type adı. Değer: yalnızca C++'ta tutulan profil.
inline const std::map<std::string, AircraftProfile>& aircraft_profiles() {
    static const std::map<std::string, AircraftProfile> table = {
        // Gerçek uçakların performans verisini burada kendin doldurabilirsin.
        {"F16", {"F-16", std::nullopt, std::nullopt}},
        {"Su57", {"Su-57", std::nullopt, std::nullopt}},

        // TAMAMEN ÖRNEK: gerçek bir uçağın veya F-16 fiziğinin performansı değil.
        {"CustomAircraft", {"Ornek ozel ucak", 1.2, 4.0}},
    };
    return table;
}

inline const AircraftProfile* find_aircraft_profile(const std::string& type) {
    const auto& table = aircraft_profiles();
    auto found = table.find(type);
    return found == table.end() ? nullptr : &found->second;
}
