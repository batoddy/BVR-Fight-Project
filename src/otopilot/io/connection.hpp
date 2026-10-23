#pragma once

#include <memory>
#include "io/messages.hpp"

// Uygulama yalnızca mesaj alır/verir; taşıma ayrıntıları .cpp içinde kalır.
class Connection
{
public:
    Connection(int argc, char* argv[]);
    ~Connection();

    // Kopyalanamaz: tek bir TCP bağlantısını temsil eder.
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    bool receive(Message& message);  // Ölçüm al; uçuş bittiyse false döndür.
    void send(const Message& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
