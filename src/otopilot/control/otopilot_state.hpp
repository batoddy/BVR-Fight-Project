#pragma once

// Otopilot durum makinesinin durumları. Atış durumları (Shot, EscapeShot, CounterShot) tek step'tir:
// atış emri verilir, manevra değişmez.
enum class OtopilotState
{
    Commit,          // düşmana uç; K1 atışa karar verir
    Shot,            // tek step: K1 atış emri
    Support,         // Crank: füzeyi destekle; sınırı K3
    EscapeShot,      // tek step: çıkmadan önce atış emri (İmha / Riskli: MAR'da), sonra Escape
    Escape,          // Out: düşman arkada kalınca K5
    Recommit,        // atış açısına dön, sonra Commit
    Departure,       // çıkış rotasını koru
    CounterShot,     // tek step: K4 karşı atış emri, sonra destek
    Defense,         // K4: Beam / Drag / Notch; tehdit bitince K5
};
