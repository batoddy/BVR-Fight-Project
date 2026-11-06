#include "control/otopilot_maneuver_test.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace
{
    struct Leg
    {
        const char* name;
        Phase phase;
        double duration_s;
        double angle_deg;
        ManeuverSide side;
        bool fire_enemy;
    };

    // Süreler fizik saniyesidir. Toplam 1155 s; ardından son Hold sürer. Snake henüz uygulanmadığı için yoktur.
    constexpr std::array<Leg, OtopilotManeuverTest::LEG_COUNT> LEGS = {{
        {"Hold",            Phase::Hold,         10,   0, ManeuverSide::Right, false},
        // {"FlyHeading +60",  Phase::FlyHeading,   60,  60, ManeuverSide::Right, false},
        // {"FlyHeading -60",  Phase::FlyHeading,   60, -60, ManeuverSide::Left,  false},
        // {"Commit",          Phase::Commit,       40,   0, ManeuverSide::Right, false},
        {"Crank",           Phase::Crank,        45,   0, ManeuverSide::Right, false},
        // {"Beam",            Phase::Beam,         60,   0, ManeuverSide::Right, false},
        // {"Offset Right 40", Phase::Offset,       45,  40, ManeuverSide::Right, false},
        // {"Offset Left 40",  Phase::Offset,       45,  40, ManeuverSide::Left,  false},
        {"Pure Pursuit",    Phase::PurePursuit,  40,   0, ManeuverSide::Right, false},
        {"Lag Pursuit",     Phase::LagPursuit,   60,   0, ManeuverSide::Right, false},
        {"Lead Pursuit",    Phase::LeadPursuit,  60,   0, ManeuverSide::Right, false},
        {"Flank +40",       Phase::Flank,       120,  40, ManeuverSide::Right, false},
        {"Flank -40",       Phase::Flank,       120, -40, ManeuverSide::Left,  false},
        {"Collision",       Phase::Collision,    90,   0, ManeuverSide::Right, false},
        {"Out",             Phase::Out,          60,   0, ManeuverSide::Right, false},
        {"Recommit",        Phase::Recommit,     60,   0, ManeuverSide::Right, false},
        {"Notch",           Phase::Notch,        90,   0, ManeuverSide::Right, true},
        {"Drag",            Phase::Drag,         90,   0, ManeuverSide::Right, true},
    }};
}

void OtopilotManeuverTest::step(Aircraft& own, Aircraft& enemy, double sim_time_s)
{
    if (!std::isfinite(sim_time_s) || (last_time_s_ && sim_time_s < *last_time_s_))
    {
        throw std::invalid_argument("Manevra test saati sonlu ve ileri yonlu olmalidir");
    }
    last_time_s_ = sim_time_s;
    bool entering = false;
    if (!leg_start_s_)
    {
        leg_start_s_ = sim_time_s;
        entry_heading_deg_ = own.kinematics.heading_deg;
        entering = true;
    }
    elapsed_s_ = sim_time_s - *leg_start_s_;
    if (!finished() && elapsed_s_ >= LEGS[leg_index_].duration_s)
    {
        ++leg_index_;
        leg_start_s_ = sim_time_s;
        elapsed_s_ = 0;
        entering = true;
    }
    if (entering)
    {
        shot_failed_ = false;
        if (!finished() && LEGS[leg_index_].fire_enemy)
        {
            shot_failed_ = !enemy.weapons.fire(enemy.profile.main_weapon);
        }
    }

    enemy_maneuvers_.fly(enemy, enemy_maneuvers_.hold);
    if (finished())
    {
        active_ = &maneuvers_.hold;
    }
    else
    {
        const Leg& leg = LEGS[leg_index_];
        active_ = &select(leg.phase, leg.angle_deg, leg.side);
    }
    maneuvers_.fly(own, *active_);
}

Maneuver& OtopilotManeuverTest::select(Phase phase, double angle_deg, ManeuverSide side)
{
    switch (phase)
    {
    case Phase::Hold: return maneuvers_.hold;
    case Phase::Out: return maneuvers_.out;
    case Phase::Commit: return maneuvers_.commit;
    case Phase::Crank: return maneuvers_.crank;
    case Phase::Beam: return maneuvers_.beam;
    case Phase::Recommit: return maneuvers_.recommit;
    case Phase::Notch: return maneuvers_.notch;
    case Phase::Drag: return maneuvers_.drag;
    case Phase::Offset:
        maneuvers_.offset.set(angle_deg, side);
        return maneuvers_.offset;
    case Phase::Flank:
        maneuvers_.flank.set(angle_deg);
        return maneuvers_.flank;
    case Phase::Collision: return maneuvers_.collision;
    case Phase::FlyHeading:
        maneuvers_.fly_heading.set(entry_heading_deg_ + angle_deg);
        return maneuvers_.fly_heading;
    case Phase::PurePursuit: return maneuvers_.pure_pursuit;
    case Phase::LagPursuit: return maneuvers_.lag_pursuit;
    case Phase::LeadPursuit: return maneuvers_.lead_pursuit;
    case Phase::Snake: break;
    }
    throw std::invalid_argument("Manevra testinde desteklenmeyen faz");
}

const char* OtopilotManeuverTest::leg_name() const
{
    if (finished()) { return "Complete / Hold"; }
    return LEGS[leg_index_].name;
}

double OtopilotManeuverTest::duration_s() const
{
    if (finished()) { return 0; }
    return LEGS[leg_index_].duration_s;
}

ManeuverStatus OtopilotManeuverTest::status(const Aircraft& own) const
{
    if (active_ == nullptr) { return ManeuverStatus::Running; }
    return active_->status(own);
}
