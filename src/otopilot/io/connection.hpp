#pragma once

#include <memory>
#include "io/messages.hpp"

// Uygulama yalnızca mesaj alır/verir; taşıma ayrıntıları .cpp içinde kalır.
class Connection
{
public:
    /// Python sunucusuna bağlanır (varsayılan 127.0.0.1:6000; adres argümanla değiştirilebilir).
    Connection(int argc, char* argv[]);
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
