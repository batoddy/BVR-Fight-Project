#pragma once

// Uçağın şu an yaptığı iş. Sayı değerleri sabit: CSV'ye ve görüntüleyiciye sayı olarak gider.
// Yeni aşama eklersen sona ekle ve görüntüleyicideki adı da ekle (BVRGym-Goruntuleyici/web/app.js PHASE_NAMES).
enum class Phase
{
    Hold = 0,    // rotayı koru
    Fire = 1,    // füze atışı
    Out = 2,     // düşmana kuyruk dön, kaç
    In = 3,      // düşmana dön
    Crank = 4,   // düşmanı radar sınırında tutarak dön
    Beam = 5,    // düşman uçağa dik uç (3/9 hattı)
};
