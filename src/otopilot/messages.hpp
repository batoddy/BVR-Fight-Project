#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include "third_party/nlohmann/json.hpp"

using Json = nlohmann::json;
inline constexpr std::size_t MAX_MESSAGE_BYTES = 1024 * 1024;

// Python connector.py içindeki Message ile aynı üç alan.
// Telde: {"kind": ..., "tick": ..., "data": {...}} + satır sonu.
struct Message
{
    std::string kind;        // "state" / "command" / "end"
    std::uint64_t tick = 0;  // adım numarası
    Json data;

    // Mesajı tek satır JSON'a çevir.
    std::string encode() const
    {
        Json value;
        value["kind"] = kind;
        value["tick"] = tick;
        value["data"] = data;
        return value.dump() + "\n";
    }

    // Tek satır JSON'u mesaja çevir; biçim yanlışsa hata ver.
    static Message decode(const std::string& line)
    {
        Json value = Json::parse(line);

        // Mesaj tam olarak kind, tick ve data alanlarından oluşmalı.
        bool shape_ok = value.is_object() && value.size() == 3;
        bool kind_ok = value.contains("kind") && value.at("kind").is_string();
        bool tick_ok = value.contains("tick") && value.at("tick").is_number_unsigned();
        bool data_ok = value.contains("data") && value.at("data").is_object();
        if (!shape_ok || !kind_ok || !tick_ok || !data_ok)
        {
            throw std::runtime_error("Mesaj kind, tick ve data alanlarindan olusmali.");
        }

        std::string kind = value.at("kind").get<std::string>();
        bool known_kind = kind == "state" || kind == "command" || kind == "end";
        if (!known_kind)
        {
            throw std::runtime_error("Mesaj turu gecersiz.");
        }

        Message message;
        message.kind = kind;
        message.tick = value.at("tick").get<std::uint64_t>();
        message.data = value.at("data");
        return message;
    }
};
