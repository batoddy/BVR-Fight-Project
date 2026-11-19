#include "io/messages.hpp"

#include <stdexcept>

std::string Message::encode() const
{
    Json value;
    value["kind"] = kind;
    value["tick"] = tick;
    value["data"] = data;
    return value.dump() + "\n";
}

Message Message::decode(const std::string& line)
{
    Json value = Json::parse(line);

    bool shape_ok = value.is_object() && value.size() == 3;   // tam olarak kind, tick ve data
    bool kind_ok = value.contains("kind") && value.at("kind").is_string();
    bool tick_ok = value.contains("tick") && value.at("tick").is_number_unsigned();
    bool data_ok = value.contains("data") && value.at("data").is_object();
    if (!shape_ok || !kind_ok || !tick_ok || !data_ok)
    {
        throw std::runtime_error("Mesaj kind, tick ve data alanlarindan olusmali.");
    }

    std::string kind = value.at("kind").get<std::string>();
    bool known_kind = kind == "hello" || kind == "state" || kind == "command" || kind == "end";
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
