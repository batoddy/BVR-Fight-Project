#pragma once

// Uçağa giden emirler. JSON bilmez; CommandWriter çevirir.

// Kalıcı uçuş hedefi: değiştirilene kadar her step aynısı gider.
struct FlightCommand
{
    double heading_deg_180 = 0;   // derece ±180, 0 kuzey, + doğu | JSON heading_deg (0..360, yazarken çevrilir)
    double altitude_m = 0;    // m, deniz seviyesinden        | JSON altitude_m
    double throttle = 0;      // 0..1                         | JSON throttle
};
