
#include "control/fsm/otopilot_fsm.hpp"

// Gazı ve g sınırını manevralar yazar; Shot yalnız atış emri verir, manevra uçmaz.
void OtopilotFSM::step(Aircraft& own)
{
    if (state_ != OtopilotState::Abort)
    {
        check_abort(own);
    }
    react_to_threat(own);

    switch (state_)
    {
    case OtopilotState::Commit: step_commit(own); break;
    // case OtopilotState::Shot: step_shot(own); break;
    // case OtopilotState::Support: step_support(own); break;
    // case OtopilotState::Trade: step_trade(own); break;
    // case OtopilotState::Defend: step_defend(own); break;
    // case OtopilotState::Out: step_out(own); break;
    // case OtopilotState::Cold: step_cold(own); break;
    // case OtopilotState::Recommit: step_recommit(own); break;
    // case OtopilotState::Chase: step_chase(own); break;
    // case OtopilotState::Abort: step_abort(own); break;
    }
    own.state = state_name(state_);   // kayıt için: step sonundaki durum
}



void OtopilotFSM::step_commit(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.commit);


}

void OtopilotFSM::check_abort(Aircraft& own)
{
    
}

void OtopilotFSM::react_to_threat(Aircraft& own)
{
    
}