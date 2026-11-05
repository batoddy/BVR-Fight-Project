#include <iostream>

#include "control/otopilot_fighter.hpp"

int main()
{
    Aircraft own, enemy;
    own.profile = aircraft_profile("F16");
    own.kinematics.altitude_ft = 30000;
    own.command.set_altitude_ft(30000).set_throttle(0.45);
    own.picture.range_3d_nm = 34;
    own.picture.bearing_deg = 0;
    for (int i = 0; i < 4; ++i)
    {
        Missile missile;
        missile.slot = i;
        missile.profile = AIM120C;
        own.weapons.missiles.push_back(missile);
    }

    OtopilotFighter pilot;
    pilot.step(own, enemy);  // Commit -> Fire
    pilot.step(own, enemy);  // Fire -> Crank
    own.weapons.missiles[0].python_id = "blue/missile/0";
    own.weapons.has_launch_order = false;
    own.picture.is_in_dor = true;
    pilot.step(own, enemy);  // Crank -> Out
    own.picture.ata_deg = 180;
    pilot.step(own, enemy);  // Out -> Recommit
    own.picture.ata_deg = 0; // Already pointing at target
    own.picture.is_in_mar = true;
    for (int i = 0; i < 20; ++i)
    {
        pilot.step(own, enemy);
    }
    std::cout << "After 20 recommit steps: phase=" << static_cast<int>(own.phase)
              << " (In=3), missiles_left=" << own.weapons.missiles_left()
              << ", in_mar=" << own.picture.is_in_mar << '\n';
    std::cout << "In maneuver can finish="
              << (In{}.status(own) == ManeuverStatus::Done) << '\n';
}
