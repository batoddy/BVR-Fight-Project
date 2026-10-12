#include "controller.hpp"

Message Controller::calculate(const Message& state) {
    // İlk ölçümde Python/YAML başlangıç hedeflerini bir kez al.
    if (targets_.empty()) {
        targets_ = state.data.at("flight_commands");
        if (!targets_.is_array() || targets_.empty())
            throw std::runtime_error("Baslangic ucus komutlari bulunamadi.");
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
