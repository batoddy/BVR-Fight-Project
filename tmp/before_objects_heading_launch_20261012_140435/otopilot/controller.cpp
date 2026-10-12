#include "controller.hpp"
#include "aircraft_profiles.hpp"
#include <iostream>

Message Controller::calculate(const Message& state) {
    // İlk ölçümde Python/YAML başlangıç hedeflerini bir kez al.
    if (targets_.empty()) {
        targets_ = state.data.at("flight_commands");
        if (!targets_.is_array() || targets_.empty())
            throw std::runtime_error("Baslangic ucus komutlari bulunamadi.");
    }



    // Python'dan sadece type adı geldi; profil bilgilerini C++ tablosundan al.
    for (const Json& aircraft : state.data.at("aircraft")) {
        std::string type = aircraft.at("type").get<std::string>();
        const AircraftProfile* profile = find_aircraft_profile(type);

        // Örnek okumayı sadece ilk ölçümde yazdır; her adımda terminali doldurma.
        if (state.tick == 0) {
            std::cout << aircraft.at("id").get<std::string>() << " | type: " << type
                      << " | profil: " << (profile ? profile->name : "bulunamadi") << '\n';

            // Profil ve sayısal değer varsa oku. Eksik veriyi sıfır sayma.
            if (profile && profile->max_mach)
                std::cout << "  Profil max Mach: " << *profile->max_mach << '\n';
            if (profile && profile->max_g)
                std::cout << "  Profil max g: " << *profile->max_g << '\n';
        }
    }

    // SENİN HESAPLAMA KODUN BURAYA GELECEK.
    // Ölçüm: state.data.at("aircraft"), state.data.at("missiles")
    // Tip adı: aircraft.at("type").get<std::string>()
    // Bu adla C++ lookup table içindeki kendi uçak profilini seçebilirsin.
    // Hedef: targets_ içindeki heading_deg, altitude_m ve throttle.
    // Örnek (kapalı):
    // for (auto& target : targets_)
    //     if (target.at("id") == "blue") target["heading_deg"] = 90.0;




    return {"command", state.tick, Json{{"aircraft", targets_}}};
}
