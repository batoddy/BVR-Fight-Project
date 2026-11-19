#pragma once

#include "model/aircraft.hpp"
#include "model/mission.hpp"

// Karar noktaları: otopilot hangi fazda olduğunu bilir, bu fonksiyonlar o fazda ne yapılacağını seçer.
// Her fonksiyon saf sorgudur: own ve mission'ı okur, seçimi ve nedenini döndürür; akışı otopilot değiştirir.
// Kurallar, metrikler ve geçiş şeması: control/KARAR_TASARIMI.md
template <typename Choice>
struct Decision
{
    Choice choice;
    const char* reason;   // karar kaydında görünen neden
};

enum class AbortChoice { Continue, Abort };
enum class ThreatResponse { Continue, CounterFire, Trade, Defend };
enum class CommitChoice { Continue, Fire, Reassess };


namespace decide
{
    Decision<AbortChoice> abort(const Aircraft& own);                                          // kesinti: füzem bitti ya da MAR ihlali
    Decision<ThreatResponse> threat_response(const Aircraft& own, const Mission& mission);     // kesinti: bana gelen güdümlü füze
    Decision<CommitChoice> commit(const Aircraft& own, const Mission& mission);                // COMMIT: ilk atış noktası, TR + mission.early_shot_nm
}
