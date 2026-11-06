#pragma once

// Yerel eksenler (NED: North, East, Down), bakan noktaya göre.
// Konum farkı NM, hız kt, normalize yön birimsizdir; vektörü tutan alanın adı birimi belirtir.
//   north: kuzeye, east: doğuya, down: aşağıya (hedef bizden alçaktaysa +)
struct Ned
{
    double north = 0;
    double east = 0;
    double down = 0;
};

inline double dot(const Ned& a, const Ned& b)   // iç çarpım: b birim vektörse a'nın b yönündeki bileşeni (hız · los_unit)
{
    return a.north * b.north + a.east * b.east + a.down * b.down;
}
