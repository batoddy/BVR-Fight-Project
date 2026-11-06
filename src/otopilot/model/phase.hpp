#pragma once

// Uçağın şu an uçtuğu manevra. Sayı değerleri sabit: CSV'ye ve görüntüleyiciye sayı olarak gider.
// Yeni manevra eklersen sona ekle ve görüntüleyicideki adı da ekle (BVRGym-Goruntuleyici/web/app.js PHASE_NAMES).
// 1 boş: eskiden Fire'dı; atış artık manevra değil, own.weapons.fire emri. Eski kayıtlar 1'i FIRE gösterir.
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
    Snake = 13,        // periyodik sağ-sol
    PurePursuit = 14,  // burun düşmanda
    LagPursuit = 15,   // burun düşmanın gerisinde
    LeadPursuit = 16,  // burun düşmanın önünde
};
