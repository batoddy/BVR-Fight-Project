#pragma once

// Otopilot durum makinesinin durumları: fighter ve hostile aynı listeyi kullanır, her biri kendine gerekeni.
// Atış durumları (FirstShot, SecondShot, Fire) tek step'tir: atış emri verilir, manevra değişmez.
enum class OtopilotState
{
    Commit,          // düşmana uç
    FirstShot,       // tek step ilk atış emri
    FirstSupport,    // Crank: füzeyi destekle
    FirstEscape,     // Out: düşman arkada kalınca biter
    Recommit,        // atış açısına dön
    SecondShot,      // tek step ikinci atış emri
    SecondSupport,   // Crank
    FinalEscape,     // Out
    Departure,       // çıkış rotasını koru
    Fire,            // tek step atış emri (hostile)
    Out,             // kaç (hostile)
    HoldAfterOut,    // kaçış rotasını koru (hostile)
    Hold,            // rotayı koru
};
