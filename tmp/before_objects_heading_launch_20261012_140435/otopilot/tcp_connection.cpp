// Windows ve TCP ayrıntıları yalnızca bu dosyada bulunur.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "connection.hpp"

struct Connection::Impl {
    SOCKET socket_ = INVALID_SOCKET;
    bool winsock_started_ = false;
    std::string buffer_;
    std::uint64_t expected_tick_ = 0;
    bool awaiting_command_ = false;
    bool finished_ = false;

    Impl(const std::string& host, unsigned short port);
    ~Impl() { close(); }
    void close();
    Message read();
    void write(const Message& message);
};

Connection::Impl::Impl(const std::string& host, unsigned short port) {
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
        std::cout << "Python'a baglandi: " << host << ':' << port << std::endl;
    }
    catch (...) {
        close();
        throw;
    }
}

void Connection::Impl::close() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (winsock_started_) {
        WSACleanup();
        winsock_started_ = false;
    }
}

void Connection::Impl::write(const Message& message) {
    std::string line = message.encode();
    if (line.size() > MAX_MESSAGE_BYTES)
        throw std::runtime_error("Mesaj 1 MiB sinirini asiyor.");

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

Message Connection::Impl::read() {
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

        char chunk[4096];
        int received = recv(socket_, chunk, sizeof(chunk), 0);
        if (received == SOCKET_ERROR)
            throw std::runtime_error("TCP alim hatasi: " + std::to_string(WSAGetLastError()));
        if (received == 0)
            throw std::runtime_error("Python baglantiyi end mesajindan once kapatti.");
        buffer_.append(chunk, static_cast<std::size_t>(received));
    }
}

Connection::Connection(int argc, char* argv[]) {
    // Adres/port ve başlangıç argümanları uygulamaya taşınmaz.
    if (argc > 3)
        throw std::runtime_error("Kullanim: main.exe [IPv4] [port]");
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    unsigned short port = 6000;
    if (argc > 2) {
        std::size_t used = 0;
        int value = std::stoi(argv[2], &used);
        if (used != std::string(argv[2]).size() || value < 1 || value > 65535)
            throw std::runtime_error("Port 1..65535 araliginda olmali.");
        port = static_cast<unsigned short>(value);
    }
    impl_ = std::make_unique<Impl>(host, port);
}

Connection::~Connection() = default;

bool Connection::receive(Message& message) {
    if (impl_->finished_)
        return false;
    if (impl_->awaiting_command_)
        throw std::runtime_error("Yeni olcumden once onceki komut gonderilmeli.");

    message = impl_->read();
    if (message.tick != impl_->expected_tick_)
        throw std::runtime_error("Olcum adim sirasi yanlis.");
    if (message.kind == "end") {
        impl_->finished_ = true;
        std::cout << "Simulasyon bitti. Son tick: " << message.tick << std::endl;
        return false;
    }
    if (message.kind != "state")
        throw std::runtime_error("Olcum mesaji bekleniyordu.");
    impl_->awaiting_command_ = true;
    return true;
}

void Connection::send(const Message& message) {
    if (!impl_->awaiting_command_ || message.kind != "command" ||
        message.tick != impl_->expected_tick_)
        throw std::runtime_error("Komut mevcut olcumun adimina ait olmali.");
    impl_->write(message);
    impl_->awaiting_command_ = false;
    ++impl_->expected_tick_;
}
