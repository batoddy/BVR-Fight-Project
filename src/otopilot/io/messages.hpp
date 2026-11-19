#pragma once

#include <cstdint>
#include <string>

// JSON'dan gizli dönüşümü kapat: değer okurken türü her zaman açık yaz, j.at("x").get<double>().
// Unutulursa derleyici hata verir. json.hpp'den önce gelmeli.
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include "third_party/nlohmann/json.hpp"

using Json = nlohmann::json;
inline constexpr std::size_t MAX_MESSAGE_BYTES = 1024 * 1024;

// Python connector.py içindeki Message ile aynı üç alan.
// Telde: {"kind": ..., "tick": ..., "data": {...}} + satır sonu.
struct Message
{
    std::string kind;        // "hello" / "state" / "command" / "end"
    std::uint64_t tick = 0;  // adım numarası
    Json data;

    std::string encode() const;                   // tek satır JSON + satır sonu
    static Message decode(const std::string& line);   // tek satır JSON'dan; biçim yanlışsa hata
};
