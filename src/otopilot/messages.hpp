#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include "third_party/nlohmann/json.hpp"

using Json = nlohmann::json;
inline constexpr std::size_t MAX_MESSAGE_BYTES = 1024 * 1024;

// Python connector.py içindeki Message ile aynı üç alan.
struct Message {
    std::string kind;
    std::uint64_t tick = 0;
    Json data;

    std::string encode() const {
        return Json{{"kind", kind}, {"tick", tick}, {"data", data}}.dump() + "\n";
    }

    static Message decode(const std::string& line) {
        Json value = Json::parse(line);
        if (!value.is_object() || value.size() != 3 ||
            !value.contains("kind") || !value.at("kind").is_string() ||
            !value.contains("tick") || !value.at("tick").is_number_unsigned() ||
            !value.contains("data") || !value.at("data").is_object()) {
            throw std::runtime_error("Mesaj kind, tick ve data alanlarindan olusmali.");
        }
        auto kind = value.at("kind").get<std::string>();
        if (kind != "state" && kind != "command" && kind != "end")
            throw std::runtime_error("Mesaj turu gecersiz.");
        return {kind, value.at("tick").get<std::uint64_t>(), value.at("data")};
    }
};
