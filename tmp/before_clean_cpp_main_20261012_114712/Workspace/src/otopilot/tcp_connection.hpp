#pragma once

// Windows'un TCP API'si. Bu başlık windows.h'den önce gelmeli.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>

#include <string>
#include "messages.hpp"

class TCPConnection {
public:
    TCPConnection(const std::string& host, unsigned short port);
    ~TCPConnection();

    TCPConnection(const TCPConnection&) = delete;
    TCPConnection& operator=(const TCPConnection&) = delete;

    Message receive();
    void send(const Message& message);

private:
    SOCKET socket_ = INVALID_SOCKET;
    bool winsock_started_ = false;
    std::string buffer_;  // Sonraki mesajdan gelen baytları da saklar.

    void close();
};
