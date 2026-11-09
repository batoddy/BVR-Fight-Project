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
enum class SupportChoice { Continue, SecondShot, Out, Abort, Reassess };
enum class TradeChoice { Continue, Support, Defend };
enum class Defense { Beam, Drag, Notch };                  // artan aciliyet sırasıyla; savunma yalnız ileri gider
enum class ReassessChoice { Abort, Chase, Cold, Out, Recommit };
enum class ColdChoice { Run, Hold, Recommit, Chase };      // Run: out yönünde kaçmaya devam, Hold: rotayı koru
enum class ChaseChoice { Continue, Fire, Commit, Out, GiveUp };

namespace decide
{
    Decision<AbortChoice> abort(const Aircraft& own);                                          // kesinti: füzem bitti ya da MAR ihlali
    Decision<ThreatResponse> threat_response(const Aircraft& own, const Mission& mission);     // kesinti: bana gelen güdümlü füze
    Decision<CommitChoice> commit(const Aircraft& own, const Mission& mission);                // COMMIT: ilk atış noktası, TR + mission.early_shot_nm
    Decision<SupportChoice> support(const Aircraft& own, const Mission& mission);              // SUPPORT: 2. atış ve destek sonu
    Decision<TradeChoice> trade(const Aircraft& own, const Mission& mission);                  // TRADE: crank'i ne kadar tutarım
    Decision<Defense> defense(const Aircraft& own, const Mission& mission);                    // DEFEND: kaçış payına göre savunma tipi
    Decision<ReassessChoice> reassess(const Aircraft& own, const Mission& mission);            // REASSESS: taahhüt bitti, sırada ne var
    Decision<ColdChoice> cold(const Aircraft& own, const Mission& mission, bool gave_up_chase); // COLD: recommit anı
    Decision<ChaseChoice> chase(const Aircraft& own);                                          // CHASE: kovala, at ya da vazgeç
}
