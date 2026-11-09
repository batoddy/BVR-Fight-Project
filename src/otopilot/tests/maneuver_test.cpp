#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "calculations/geometry.hpp"
#include "common/angles.hpp"
#include "common/formulas.hpp"
#include "common/units.hpp"
#include "control/maneuver.hpp"
#include "model/missile.hpp"
#include "model/world.hpp"

namespace
{
    void require(bool condition, const char* label)
    {
        if (!condition)
        {
            throw std::runtime_error(label);
        }
    }

    void near_angle(double actual_deg, double expected_deg, const char* label)
    {
        require(std::isfinite(actual_deg) && std::abs(wrap_180(actual_deg - expected_deg)) < 1e-6, label);
    }

    void crank_and_threat_status()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        Crank crank;
        own.picture.ta_deg = 20;
        crank.enter(own);
        own.picture.ata_deg = 0;
        require(crank.status(own) == ManeuverStatus::Running, "crank is not satisfied just because radar sees target");
        own.picture.ata_deg = 57;
        require(crank.is_satisfied(own), "crank reaches selected angle");
        own.picture.ata_deg = 55;
        require(crank.is_satisfied(own), "crank includes tolerance boundary");
        own.picture.ata_deg = -57;
        require(!crank.is_satisfied(own), "opposite crank side is not satisfied");
        own.picture.ata_deg = 60;
        require(crank.status(own) == ManeuverStatus::Running, "crank loses satisfied outside target angle");
        own.picture.ta_deg = -20;
        crank.enter(own);
        own.picture.ata_deg = -57;
        require(crank.is_satisfied(own), "right crank has negative target ATA");

        Drag drag;
        Notch notch;
        Missile threat;
        own.kinematics.heading_deg = 0;
        own.picture.threat = &threat;
        threat.kinematics.lat_deg = 1;   // füze tam önümde
        require(drag.status(own) == ManeuverStatus::Running && notch.status(own) == ManeuverStatus::Running,
                "missile ahead satisfies neither drag nor notch");
        threat.kinematics.lat_deg = -1;  // füze tam arkamda
        require(drag.is_satisfied(own), "drag satisfied with missile behind");
        threat.kinematics.lat_deg = 0;
        threat.kinematics.lon_deg = 1;   // füze 3 hizamda
        require(notch.is_satisfied(own) && !drag.is_satisfied(own), "notch satisfied with missile on beam");

        own.picture.threat = nullptr;    // füze yok: düşman uçağa göre
        own.picture.ata_deg = -90;
        require(notch.is_satisfied(own), "notch uses enemy aircraft without missile");
        own.picture.ata_deg = 179;
        require(drag.is_satisfied(own) && !notch.is_satisfied(own), "drag uses enemy aircraft without missile");
        own.picture.bearing_deg = 30;
        near_angle(drag.update(own).heading_deg(), -150, "drag turns tail to enemy aircraft without missile");
        own.picture.ata_deg = 20;
        near_angle(notch.update(own).heading_deg(), -60, "notch beams enemy aircraft without missile");
        require(drag.status(own) != ManeuverStatus::Done && notch.status(own) != ManeuverStatus::Done,
                "drag and notch never signal done");
    }

    void flank_status_and_configuration()
    {
        Aircraft own;
        Flank flank;
        flank.set(40, 3);
        own.picture.ata_deg = 40;
        own.picture.ta_deg = 10;
        require(!flank.is_satisfied(own), "flank tests target aspect rather than own nose angle");
        own.picture.ta_deg = 37;
        require(flank.is_satisfied(own), "flank includes lower angle boundary");
        own.picture.ta_deg = 43;
        require(flank.is_satisfied(own), "flank includes upper angle boundary");
        own.picture.ta_deg = 44;
        require(flank.status(own) == ManeuverStatus::Running, "overshooting flank loses satisfied rather than becoming done");
        own.picture.ta_deg = -40;
        require(!flank.is_satisfied(own), "flank distinguishes target side");
        flank.set(-40);
        require(flank.is_satisfied(own), "flank can hold left side");
        flank.set(179, 3);
        own.picture.ta_deg = -179;
        require(flank.is_satisfied(own), "flank angle error wraps across 180");
        flank.set(320, 0);
        own.picture.ta_deg = -40;
        require(flank.is_satisfied(own), "flank wraps configured angle");

        for (double invalid_tolerance : {-1.0, 181.0, std::numeric_limits<double>::infinity()})
        {
            bool rejected = false;
            try { flank.set(20, invalid_tolerance); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected && flank.is_satisfied(own), "invalid setup leaves previous configuration intact");
        }
        bool rejected = false;
        try { flank.set(std::numeric_limits<double>::quiet_NaN()); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "flank rejects non-finite target angle");
    }

    void flank_guidance()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        own.command.set_altitude_ft(30000);
        own.kinematics.heading_deg = 70;
        own.picture.bearing_deg = 180;
        own.picture.range_2d_nm = 10;
        own.picture.los_nm.north = -10;
        own.picture.enemy_velocity_ned_kt.north = 450;
        own.picture.ta_deg = 0;
        Flank flank;
        flank.set(40);
        flank.enter(own);   // menzil 10 NM'de kilitlenir
        const FlightCommand right = flank.update(own);
        require(right.heading_deg() > 0, "flank moves right toward positive target aspect");
        flank.set(-40);
        const FlightCommand left = flank.update(own);
        require(left.heading_deg() < 0, "flank moves left toward negative target aspect");
        require(right.altitude_ft() == 30000 && right.throttle() == throttle::DEFAULT, "flank preserves altitude and selects throttle");
        own.profile.max_g = 2;
        require(*flank.update(own).g_limit() == 2, "flank respects aircraft g limit");

        own.picture.ta_deg = 40;
        own.picture.bearing_deg = -140;
        own.picture.los_nm.north = -10 * std::cos(40 * DEG_TO_RAD);
        own.picture.los_nm.east = -10 * std::sin(40 * DEG_TO_RAD);
        flank.set(40);
        require(flank.is_satisfied(own), "flank is satisfied at target aspect");
        near_angle(flank.update(own).heading_deg(), 0, "at target aspect flank follows moving enemy rather than turning toward it");
        own.picture.enemy_velocity_ned_kt.north = 0;
        near_angle(flank.update(own).heading_deg(), 70, "zero guidance velocity retains current heading");
    }

    void hold_locks_and_reenters()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        own.kinematics.heading_deg = 179;
        own.kinematics.altitude_ft = 30000;
        Maneuvers maneuvers;
        maneuvers.fly(own, maneuvers.hold);
        require(maneuvers.hold.is_satisfied(own), "hold is satisfied at entry");
        own.kinematics.heading_deg = -179;
        require(maneuvers.hold.is_satisfied(own), "hold heading tolerance wraps across 180");
        own.kinematics.altitude_ft = 30101;
        require(!maneuvers.hold.is_satisfied(own), "hold detects altitude drift independently");
        own.kinematics.heading_deg = 160;
        own.kinematics.altitude_ft = 30000;
        require(!maneuvers.hold.is_satisfied(own), "hold detects heading drift independently");
        own.kinematics.altitude_ft = 28000;
        maneuvers.fly(own, maneuvers.hold);
        near_angle(own.command.heading_deg(), 179, "continuous hold keeps entry heading");
        require(own.command.altitude_ft() == 30000, "continuous hold corrects to entry altitude");
        maneuvers.fly(own, maneuvers.commit);
        maneuvers.fly(own, maneuvers.hold);
        near_angle(own.command.heading_deg(), 160, "hold reentry captures new heading");
        require(own.command.altitude_ft() == 28000, "hold reentry captures new altitude");
    }

    void offset_and_heading_status()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        own.picture.bearing_deg = 170;
        Offset offset;
        offset.set(40, ManeuverSide::Right);
        near_angle(offset.update(own).heading_deg(), -150, "offset wraps right heading");
        own.picture.ata_deg = -40;
        require(offset.is_satisfied(own), "right offset has negative target ATA");
        own.picture.ata_deg = 40;
        require(!offset.is_satisfied(own), "opposite offset side is not satisfied");
        offset.set(40, ManeuverSide::Left);
        near_angle(offset.update(own).heading_deg(), 130, "offset left heading");
        require(offset.is_satisfied(own), "left offset has positive target ATA");
        own.picture.los_rate_deg_s = 2;
        require(offset.update(own).heading_deg() > 130, "offset leads moving LOS");
        for (double invalid_deg : {-1.0, 181.0, std::numeric_limits<double>::quiet_NaN()})
        {
            bool rejected = false;
            try { offset.set(invalid_deg, ManeuverSide::Right); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected && offset.is_satisfied(own), "offset validates before changing configuration");
        }

        FlyHeading heading;
        heading.set(181);
        own.kinematics.heading_deg = 179;
        require(heading.is_satisfied(own), "fly heading uses wrapped tolerance");
        own.kinematics.heading_deg = 175;
        require(!heading.is_satisfied(own), "fly heading loses satisfied after drift");
        near_angle(heading.update(own).heading_deg(), -179, "fly heading keeps configured course");

        Beam beam;
        own.picture.ata_deg = 20;
        beam.enter(own);
        own.picture.ata_deg = 90;
        require(beam.is_satisfied(own), "beam selected side is satisfied");
        own.picture.ata_deg = -90;
        require(!beam.is_satisfied(own), "beam opposite side is not satisfied");
        Commit commit;
        own.picture.ata_deg = 2;
        require(commit.is_satisfied(own), "commit nose alignment is satisfied");
        own.picture.ata_deg = 3;
        require(!commit.is_satisfied(own), "commit alignment can be lost");
    }

    void pursuit_and_collision_status()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        own.picture.bearing_deg = 170;
        own.picture.los_rate_deg_s = 1;
        own.picture.enemy_velocity_ned_kt.east = -100;
        Pursuit lead, lag, pure;
        lead.set(PursuitMode::Lead);
        lag.set(PursuitMode::Lag);
        pure.set(PursuitMode::Pure);
        near_angle(lead.update(own).heading_deg(), -170, "lead crosses wrap boundary");
        near_angle(lag.update(own).heading_deg(), 155, "lag is behind positive LOS motion");
        near_angle(pure.update(own).heading_deg(), 170, "pure tracks bearing");
        own.kinematics.heading_deg = -170;
        require(lead.is_satisfied(own), "lead tests its own target course");
        own.picture.los_rate_deg_s = -1;
        require(!lead.is_satisfied(own), "reversed LOS changes lead side in the original implementation");
        own.picture.enemy_velocity_ned_kt.east = 100;
        require(!lead.is_satisfied(own), "lead heading is not aligned after LOS reversal");
        near_angle(lead.update(own).heading_deg(), 150, "lead follows negative LOS motion");
        own.picture.los_rate_deg_s = 0;
        own.picture.enemy_velocity_ned_kt = {};
        own.kinematics.heading_deg = 170;
        require(lead.is_satisfied(own) && lag.is_satisfied(own) && pure.is_satisfied(own), "zero LOS motion uses pure course");
        require(std::abs(geometry::lateral_speed_kt(0, Ned{100, 200, 0}) - 200) < 1e-9,
                "lateral speed is target east component when looking north");
        require(std::abs(geometry::lateral_speed_kt(90, Ned{100, 200, 0}) + 100) < 1e-9,
                "lateral speed is negative target north component when looking east");

        Collision collision;
        own.picture.range_2d_nm = 20;
        own.picture.closure_kt = -100;
        require(!collision.is_satisfied(own), "constant bearing while separating is not collision success");
        own.picture.closure_kt = 100;
        own.kinematics.heading_deg = 150;
        require(collision.is_satisfied(own), "constant bearing while closing is satisfied");
        near_angle(collision.update(own).heading_deg(), 150, "collision retains intercept course at zero LOS motion");
        own.picture.los_rate_deg_s = 0.2;
        require(!collision.is_satisfied(own), "collision detects LOS drift");
        require(collision.update(own).heading_deg() > 150, "collision corrects toward LOS drift");
    }

    void explicit_g_limits()
    {
        Aircraft own;
        own.profile = aircraft_profile("F16");
        own.profile.max_g = 2;
        own.kinematics.altitude_ft = 30000;
        Missile threat;
        threat.kinematics.lat_deg = 1;
        own.picture.threat = &threat;
        Maneuvers maneuvers;
        Maneuver* implemented[] = {&maneuvers.hold, &maneuvers.out, &maneuvers.commit, &maneuvers.recommit,
            &maneuvers.crank, &maneuvers.beam, &maneuvers.notch, &maneuvers.drag, &maneuvers.offset,
            &maneuvers.flank, &maneuvers.collision, &maneuvers.fly_heading, &maneuvers.pursuit};
        for (Maneuver* maneuver : implemented)
        {
            own.command.set_g_limit(9, 9);
            maneuvers.fly(own, *maneuver);
            require(own.command.g_limit().has_value() && *own.command.g_limit() == 2,
                    "each implemented maneuver replaces previous g limit and respects profile max");
        }
        own.picture.threat = nullptr;
        require(*maneuvers.notch.update(own).g_limit() == 2 && *maneuvers.drag.update(own).g_limit() == 2,
                "threat maneuvers set g limits even without a threat");
    }

    void advance(Kinematics& kinematics, double time_s)
    {
        const double earth_radius_nm = meter_to_nmile(6371000.0);
        kinematics.lat_deg += travel_distance_nm(kinematics.velocity_ned_kt.north, time_s) / earth_radius_nm * RAD_TO_DEG;
        kinematics.lon_deg += travel_distance_nm(kinematics.velocity_ned_kt.east, time_s)
                              / (earth_radius_nm * std::cos(kinematics.lat_deg * DEG_TO_RAD)) * RAD_TO_DEG;
    }

    // Yalın kinematik kontrol: aynı hızdaki hareketli hedefte açıya yaklaşma ve açı bozulunca yeniden edinme.
    void flank_tracks_moving_target(double target_ta_deg)
    {
        World world;
        Aircraft& own = world.fighter;
        Aircraft& enemy = world.hostile;
        own.id = "blue";
        enemy.id = "red";
        own.profile = enemy.profile = aircraft_profile("F16");
        own.kinematics.tas_kt = enemy.kinematics.tas_kt = 450;
        own.kinematics.altitude_ft = enemy.kinematics.altitude_ft = 30000;
        enemy.kinematics.velocity_ned_kt.north = 450;
        own.kinematics.lat_deg = 8.0 / meter_to_nmile(6371000.0) * RAD_TO_DEG;
        Flank flank;
        flank.set(target_ta_deg);
        world.update();
        flank.enter(own);   // menzil 8 NM'de kilitlenir
        for (int step = 0; step < 900; ++step)
        {
            world.update();
            if (step == 450)
            {
                require(flank.is_satisfied(own), "moving target flank converges to configured side");
                flank.set(-target_ta_deg);
                require(!flank.is_satisfied(own), "new flank target clears satisfaction without a restart");
            }
            own.command = flank.update(own);
            own.kinematics.heading_deg = own.command.heading_deg();
            own.kinematics.velocity_ned_kt.north = 450 * std::cos(own.kinematics.heading_deg * DEG_TO_RAD);
            own.kinematics.velocity_ned_kt.east = 450 * std::sin(own.kinematics.heading_deg * DEG_TO_RAD);
            advance(own.kinematics, 1);
            advance(enemy.kinematics, 1);
        }
        world.update();
        require(flank.is_satisfied(own), "flank reacquires opposite angle while target moves");
        require(flank.status(own) != ManeuverStatus::Done, "holding flank never signals done");
    }
}

int main()
{
    try
    {
        crank_and_threat_status();
        flank_status_and_configuration();
        flank_guidance();
        hold_locks_and_reenters();
        offset_and_heading_status();
        pursuit_and_collision_status();
        explicit_g_limits();
        flank_tracks_moving_target(40);
        flank_tracks_moving_target(-40);
        std::cout << "Maneuver status and flank tracking checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
