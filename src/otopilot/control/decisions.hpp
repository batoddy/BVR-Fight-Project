#pragma once

#include "model/aircraft.hpp"
#include "model/mission.hpp"

// Karar noktaları: akış (otopilot) belli, yalnız kritik anlarda görev konseptine göre seçim yapılır.
// Her fonksiyon saf sorgudur: own ve mission'ı okur, seçimi ve nedenini döndürür; akışı otopilot değiştirir.
template <typename Choice>
struct Decision
{
    Choice choice;
    const char* reason;   // karar kaydında görünen neden
};

enum class FirstShot { Wait, Fire };                    // K1
enum class AfterShot { Leave, Crank, Commit };          // K2: L&L (Out) / destek
enum class SupportLimit { Continue, Escape, FireAndEscape };   // K3
enum class ThreatResponse { Continue, CounterFire, Beam, Drag, Notch };   // K4; savunmalar artan aciliyet sırasıyla
enum class AfterEscape { Recommit, Depart, Wait };      // K5; Wait: kaçış yönünde kal

namespace decide
{
    // K1: atış ne zaman. Commit'te her step sorulur; Fire dönünce bir kez atılır.
    Decision<FirstShot> first_shot(const Aircraft& own, const Mission& mission);
    Decision<SupportLimit> support_limit(const Aircraft& own, const Mission& mission);   // K3: ilk füze havada, destek ne zaman biter
    Decision<ThreatResponse> threat_response(const Aircraft& own, const Mission& mission);   // K4: bana füze geliyor; savunma dışında her step
    Decision<ThreatResponse> defense(const Aircraft& own, const Mission& mission);   // K4: savunmadayken füzeye kalan süreye göre Beam/Drag/Notch
    Decision<AfterEscape> after_escape(const Aircraft& own, const Mission& mission);   // K5: savunma bitti

    // --- İskelet: henüz akışa bağlı değil ---
    Decision<AfterShot> after_shot(const Aircraft& own, const Mission& mission);              // K2: atıştan sonra
}
