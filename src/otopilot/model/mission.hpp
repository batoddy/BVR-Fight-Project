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
