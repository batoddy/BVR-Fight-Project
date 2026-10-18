#include "maneuver.hpp"
#include "util_functions.hpp"

FlightCommand Hold::compute(const Aircraft& fighter, const FlightCommand& current)
{
    (void)fighter;     // hold ölçüme bakmaz
    return current;    // aynı yön, irtifa ve gaz
}

FlightCommand Crank::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalacak; yalnızca yön değişir

    // TODO: crank hesabı. Amaç: düşmanı burnun belirli bir açıda yanında tutmak (gimbal sınırından pay bırakarak).
    // Elindekiler:
    //   fighter.enemy.bearing_deg              karşı uçağın yönü, kuzeye göre (±180)
    //   fighter.enemy.ata_deg                  karşı uçağın açısı, burnuma göre (+ sağımda)
    //   fighter.kinematics.heading_deg_180     kendi burun yönüm, kuzeye göre
    //   fighter.profile.radar_gimbal_half_deg  radarın burundan sağa/sola tarama sınırı
    // İki eşdeğer yazım (hedef_ata: sağa crank'te -50 gibi):
    //   command.heading_deg_180 = wrap_180(fighter.enemy.bearing_deg - hedef_ata);
    //   command.heading_deg_180 = wrap_180(fighter.kinematics.heading_deg_180 + (fighter.enemy.ata_deg - hedef_ata));

    double crank_error = fighter.profile.radar_gimbal_deg/2 - fighter.enemy.ata_deg;
    command.heading_deg_180 = wrap_180(fighter.kinematics.heading_deg_180 + crank_error); 

    return command;
}
