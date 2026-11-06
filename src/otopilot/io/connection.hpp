#pragma once

#include <memory>
#include <string>

#include "io/messages.hpp"

// Uygulama yalnızca mesaj alır/verir; taşıma ayrıntıları .cpp içinde kalır.
class Connection
{
public:
    /// Python sunucusuna bağlanır. host: IPv4 adresi; port: 1..65535 (main.cpp komut satırından okur).
    Connection(const std::string& host, unsigned short port);
    ~Connection();

    // Kopyalanamaz: tek bir TCP bağlantısını temsil eder.
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /// Ölçüm al; uçuş bittiyse ("end") false döner.
    bool receive(Message& message);

    /// Mesajı Python'a gönder.
    void send(const Message& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
