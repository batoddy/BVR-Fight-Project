#include "control/fsm/decisions_fsm.hpp"
#include "control/maneuvers/maneuvers.hpp"
#include "control/legacy/otopilot_state.hpp"
#include "model/aircraft.hpp"

class OtopilotFSM 
{
public:
        void step(Aircraft& own);                          // yazdıkları: own.command, own.phase, own.decision, atış emri

private:
    void step_commit(Aircraft& own);
    void check_abort(Aircraft& own);
    void react_to_threat(Aircraft& own);

    Maneuvers maneuvers_;
    OtopilotState state_ = OtopilotState::Commit;
};