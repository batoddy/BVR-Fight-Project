#pragma once

#include "control/maneuvers/maneuver.hpp"
#include "control/maneuvers/hold.hpp"
#include "control/maneuvers/out.hpp"
#include "control/maneuvers/commit.hpp"
#include "control/maneuvers/recommit.hpp"
#include "control/maneuvers/crank.hpp"
#include "control/maneuvers/beam.hpp"
#include "control/maneuvers/notch.hpp"
#include "control/maneuvers/drag.hpp"
#include "control/maneuvers/offset.hpp"
#include "control/maneuvers/flank.hpp"
#include "control/maneuvers/fly_heading.hpp"
#include "control/maneuvers/pursuit.hpp"

// MANEUVERS  —  bir otopilotun manevra nesneleri

// Yeni manevra: maneuvers/ altına kendi .hpp/.cpp'sini aç, buraya include ve bir üye ekle.
class Maneuvers
{
public:
    void fly(Aircraft& own, Maneuver& maneuver);   // manevra değiştiyse enter() bir kez; sonra fazı ve komutu yazar

    Hold hold;                                     // rotayı koru
    Out out;                                       // kaç
    Commit commit;                                 // düşmana doğru düz uç
    Recommit recommit;                             // atış açısına dön
    Crank crank;                                   // radar sınırında yanal kaçış
    Beam beam;                                     // düşmana dik uç

    Notch notch;                                   // füzeye dik uç
    Drag drag;                                     // füzeye sırt dön
    Pursuit pursuit;                               // takip: Pure, Lag, Lead ya da Collision (set)
    FlyHeading fly_heading;                        // sabit yön
    Flank flank;                                   // düşmanın verilen yan açısını koru
    Offset offset;                                 // düşman yönünden açı kadar yana

private:
    Maneuver* active_ = nullptr;                   // son uçurulan manevra; ilk fly() da giriş sayılır
};
