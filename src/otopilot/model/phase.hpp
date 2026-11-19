#pragma once

// Uçağın şu an uçtuğu manevra. Kayda ve görüntüleyiciye adıyla gider (phase_name).
// Yeni manevra eklersen phase_name'e de ekle.
// 1 boş: eskiden Fire'dı; atış artık manevra değil, own.weapons.fire emri. 13 boş: eskiden Snake'ti (kaldırıldı).
enum class Phase
{
    Hold = 0,          // rotayı koru
    Out = 2,           // düşmana kuyruk dön, kaç
    Commit = 3,        // düşmana doğru düz uç
    Crank = 4,         // düşmanı radar sınırında tutarak dön
    Beam = 5,          // düşman uçağa dik uç (3/9 hattı)
    Recommit = 6,      // düşmana dönüp atış açısına gel
    Notch = 7,         // füzeye dik uç
    Drag = 8,          // füzeye sırt dön
    Offset = 9,        // düşman yönünden verilen açı kadar yana uç
    Flank = 10,        // düşmanın burnuna göre yan açıda kal
    Collision = 11,    // LOS hızını sıfırla: çarpışma rotası
    FlyHeading = 12,   // verilen sabit yöne uç
    PurePursuit = 14,  // burun düşmanda
    LagPursuit = 15,   // burun düşmanın gerisinde
    LeadPursuit = 16,  // burun düşmanın önünde
};

inline const char* phase_name(Phase phase)   // kayıttaki ad: enum adı büyük harf, kelimeler '_' ile
{
    switch (phase)
    {
        case Phase::Hold: return "HOLD";
        case Phase::Out: return "OUT";
        case Phase::Commit: return "COMMIT";
        case Phase::Crank: return "CRANK";
        case Phase::Beam: return "BEAM";
        case Phase::Recommit: return "RECOMMIT";
        case Phase::Notch: return "NOTCH";
        case Phase::Drag: return "DRAG";
        case Phase::Offset: return "OFFSET";
        case Phase::Flank: return "FLANK";
        case Phase::Collision: return "COLLISION";
        case Phase::FlyHeading: return "FLY_HEADING";
        case Phase::PurePursuit: return "PURE_PURSUIT";
        case Phase::LagPursuit: return "LAG_PURSUIT";
        case Phase::LeadPursuit: return "LEAD_PURSUIT";
    }
    return "UNKNOWN";   // buraya gelinmez; eksik değeri -Wswitch uyarır
}
