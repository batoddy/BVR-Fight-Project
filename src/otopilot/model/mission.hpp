#pragma once

// Görev konsepti: karar noktaları (control/decisions) eşiklerini buna göre seçer. Şimdilik C++'ta sabit.
enum class MissionGoal
{
    Repel,     // püskürt: düşman dönerse yeterli
    Destroy,   // imha: isabet öncelikli, daha yakında atılır
};

// Mühimmat idaresi de bunun içindedir: güvenli tutumlu, riskli bol kullanır.
enum class RiskLevel
{
    Safe,         // erken at, sınırlardan pay bırak
    Balanced,
    Aggressive,   // yakında at, sınıra dayan
};

struct Mission
{
    MissionGoal goal = MissionGoal::Destroy;
    RiskLevel risk = RiskLevel::Balanced;
    bool leave_after_support = false;   // L&L: destek bitince out yerine ABORT, savaşa dönülmez
    double early_shot_nm = 0.0;         // NM, COMMIT'te TR'nin bu kadar dışından at (ör. Güvenli görevde 3); 0: TR'de
};

inline const char* goal_name(MissionGoal goal)   // karar kaydı için
{
    return goal == MissionGoal::Repel ? "Püskürt" : "İmha";
}

inline const char* risk_name(RiskLevel risk)     // karar kaydı için
{
    switch (risk)
    {
    case RiskLevel::Safe: return "Güvenli";
    case RiskLevel::Balanced: return "Dengeli";
    case RiskLevel::Aggressive: return "Riskli";
    }
    return "?";
}
