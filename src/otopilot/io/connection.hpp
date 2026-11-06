#pragma once

#include <memory>
#include <string>

#include "io/messages.hpp"

// Uygulama yalnızca mesaj alır/verir; taşıma ayrıntıları .cpp içinde kalır.
class Connection
{
public:
    Connection(const std::string& host, unsigned short port);   // Python sunucusuna bağlanır (main.cpp adresi komut satırından okur)
    ~Connection();

    Connection(const Connection&) = delete;                      // kopyalanamaz: tek bir TCP bağlantısı
    Connection& operator=(const Connection&) = delete;

    bool receive(Message& message);                              // ölçüm al; uçuş bittiyse ("end") false
    void send(const Message& message);                           // mesajı Python'a gönder

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
