#pragma once

// Otopilot durum makinesinin fazları. Akış ve kurallar: control/KARAR_TASARIMI.md
// Shot faz değil eylemdir: atış emri kabul edilene kadar tekrar denenir, manevra değişmez, sonra Support.
// REASSESS durum değildir: taahhüt bitince Otopilot::reassess anında yeni fazı seçer.
enum class OtopilotState
{
    Commit,      // düşmana git, atış noktasını bekle
    Shot,        // atış emri; sonra Support
    Support,     // füzem havada, tehdit yok: düşman hot ise crank, cold ise kovalama
    Trade,       // iki füze havada, benimki pitbull değil: crank
    Defend,      // tehdit var, destek yok: Beam → Drag → Notch
    Out,         // düşmanı arkaya al
    Cold,        // cold ops: MRR'ye açıl, recommit anını bekle
    Recommit,    // hot'a dön
    Chase,       // İmha, düşman kaçıyor: kovala
    Abort,       // kalıcı çıkış: out, recommit yok
};
