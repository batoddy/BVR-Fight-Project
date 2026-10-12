#include "tcp_connection.hpp"

#include <ws2tcpip.h>
#include <algorithm>
#include <stdexcept>

TCPConnection::TCPConnection(const std::string& host, unsigned short port) {
    WSADATA information{};
    int result = WSAStartup(MAKEWORD(2, 2), &information);
    if (result != 0)
        throw std::runtime_error("WSAStartup hatasi: " + std::to_string(result));
    winsock_started_ = true;

    try {
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET)
            throw std::runtime_error("Socket olusturulamadi: " + std::to_string(WSAGetLastError()));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (InetPtonA(AF_INET, host.c_str(), &address.sin_addr) != 1)
            throw std::runtime_error("IPv4 adresi gecersiz: " + host);

        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            throw std::runtime_error("Python'a baglanilamadi: " + std::to_string(WSAGetLastError()));

        int enabled = 1;
        if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR)
            throw std::runtime_error("TCP_NODELAY ayarlanamadi: " + std::to_string(WSAGetLastError()));
    }
    catch (...) {
        close();
        throw;
    }
}

TCPConnection::~TCPConnection() {
    close();
}

void TCPConnection::close() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (winsock_started_) {
        WSACleanup();
        winsock_started_ = false;
    }
}

void TCPConnection::send(const Message& message) {
    std::string line = message.encode();
    if (line.size() > MAX_MESSAGE_BYTES)
        throw std::runtime_error("Mesaj 1 MiB sinirini asiyor.");

    // send() daha az bayt yazabilir; hepsi gönderilene kadar devam et.
    std::size_t offset = 0;
    while (offset < line.size()) {
        int length = static_cast<int>(std::min<std::size_t>(line.size() - offset, 4096));
        int sent = ::send(socket_, line.data() + offset, length, 0);
        if (sent == SOCKET_ERROR)
            throw std::runtime_error("TCP gonderim hatasi: " + std::to_string(WSAGetLastError()));
        if (sent == 0)
            throw std::runtime_error("TCP baglantisi kapandi.");
        offset += static_cast<std::size_t>(sent);
    }
}

Message TCPConnection::receive() {
    while (true) {
        auto newline = buffer_.find('\n');
        if (newline != std::string::npos) {
            if (newline + 1 > MAX_MESSAGE_BYTES)
                throw std::runtime_error("Mesaj 1 MiB sinirini asiyor.");
            std::string line = buffer_.substr(0, newline);
            buffer_.erase(0, newline + 1);
            return Message::decode(line);
        }
        if (buffer_.size() >= MAX_MESSAGE_BYTES)
            throw std::runtime_error("Mesaj tamamlanmadi veya boyut sinirini asiyor.");

        // Tek mesaj bölünebilir; birkaç mesaj da birlikte gelebilir.
        char chunk[4096];
        int received = recv(socket_, chunk, sizeof(chunk), 0);
        if (received == SOCKET_ERROR)
            throw std::runtime_error("TCP alim hatasi: " + std::to_string(WSAGetLastError()));
        if (received == 0)
            throw std::runtime_error("Python baglantiyi end mesajindan once kapatti.");
        buffer_.append(chunk, static_cast<std::size_t>(received));
    }
}
