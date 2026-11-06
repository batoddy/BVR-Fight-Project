// Windows ve TCP ayrıntıları yalnızca bu dosyada bulunur.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "io/connection.hpp"

struct Connection::Impl
{
    SOCKET socket_ = INVALID_SOCKET;
    bool winsock_started_ = false;
    std::string buffer_;               // gelmiş ama henüz satıra ayrılmamış baytlar
    std::uint64_t expected_tick_ = 0;  // beklenen ölçüm adımı
    bool awaiting_command_ = false;    // ölçüm alındı, komut henüz gönderilmedi
    bool finished_ = false;            // Python "end" gönderdi

    Impl(const std::string& host, unsigned short port);

    ~Impl()
    {
        close();
    }

    void close();
    Message read();
    void write(const Message& message);
};

Connection::Impl::Impl(const std::string& host, unsigned short port)   // Winsock'u başlat, Python sunucusuna bağlan
{
    WSADATA information{};
    int result = WSAStartup(MAKEWORD(2, 2), &information);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup hatasi: " + std::to_string(result));
    }
    winsock_started_ = true;

    try
    {
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET)
        {
            throw std::runtime_error("Socket olusturulamadi: " + std::to_string(WSAGetLastError()));
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);

        if (InetPtonA(AF_INET, host.c_str(), &address.sin_addr) != 1)
        {
            throw std::runtime_error("IPv4 adresi gecersiz: " + host);
        }

        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        {
            std::string code = std::to_string(WSAGetLastError());
            throw std::runtime_error("Python'a baglanilamadi (" + code + "). Once sim_main.py'yi baslat.");
        }

        int enabled = 1;   // TCP_NODELAY: küçük mesajlar beklemeden gitsin (Nagle kapalı)
        const char* option = reinterpret_cast<const char*>(&enabled);
        if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, option, sizeof(enabled)) == SOCKET_ERROR)
        {
            throw std::runtime_error("TCP_NODELAY ayarlanamadi: " + std::to_string(WSAGetLastError()));
        }

        std::cout << "Python'a baglandi: " << host << ':' << port << std::endl;
    }
    catch (...)
    {
        close();
        throw;
    }
}

void Connection::Impl::close()   // socket'i ve Winsock'u kapat; iki kez çağrılması sorun değil
{
    if (socket_ != INVALID_SOCKET)
    {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (winsock_started_)
    {
        WSACleanup();
        winsock_started_ = false;
    }
}

void Connection::Impl::write(const Message& message)   // tek satır JSON, tamamı gidene kadar
{
    std::string line = message.encode();
    if (line.size() > MAX_MESSAGE_BYTES)
    {
        throw std::runtime_error("Mesaj 1 MiB sinirini asiyor.");
    }

    std::size_t offset = 0;
    while (offset < line.size())   // send() bir seferde tamamını göndermeyebilir
    {
        const char* remaining = line.data() + offset;
        int length = static_cast<int>(line.size() - offset);   // en fazla 1 MiB
        int sent = ::send(socket_, remaining, length, 0);

        if (sent == SOCKET_ERROR)
        {
            throw std::runtime_error("TCP gonderim hatasi: " + std::to_string(WSAGetLastError()));
        }
        if (sent == 0)
        {
            throw std::runtime_error("TCP baglantisi kapandi.");
        }

        offset += static_cast<std::size_t>(sent);
    }
}

// TCP akıştır: bir satır parça parça ya da iki satır birlikte gelebilir. Tam bir satır gelene kadar okunur.
Message Connection::Impl::read()
{
    while (true)
    {
        std::size_t newline = buffer_.find('\n');
        if (newline != std::string::npos)   // tamponda tam satır var: onu döndür, kalanı sakla
        {
            if (newline + 1 > MAX_MESSAGE_BYTES)
            {
                throw std::runtime_error("Mesaj 1 MiB sinirini asiyor.");
            }

            std::string line = buffer_.substr(0, newline);
            buffer_.erase(0, newline + 1);
            return Message::decode(line);
        }

        if (buffer_.size() >= MAX_MESSAGE_BYTES)
        {
            throw std::runtime_error("Mesaj tamamlanmadi veya boyut sinirini asiyor.");
        }

        char chunk[4096];   // satır henüz tamamlanmadı: biraz daha oku
        int received = recv(socket_, chunk, sizeof(chunk), 0);

        if (received == SOCKET_ERROR)
        {
            throw std::runtime_error("TCP alim hatasi: " + std::to_string(WSAGetLastError()));
        }
        if (received == 0)
        {
            throw std::runtime_error("Python baglantiyi end mesajindan once kapatti.");
        }

        buffer_.append(chunk, static_cast<std::size_t>(received));
    }
}

Connection::Connection(const std::string& host, unsigned short port)
    : impl_(std::make_unique<Impl>(host, port))
{
}

Connection::~Connection() = default;

bool Connection::receive(Message& message)   // Python "end" gönderdiyse false
{
    if (impl_->finished_)
    {
        return false;
    }
    if (impl_->awaiting_command_)
    {
        throw std::runtime_error("Yeni olcumden once onceki komut gonderilmeli.");
    }

    message = impl_->read();

    if (message.tick != impl_->expected_tick_)
    {
        throw std::runtime_error("Olcum adim sirasi yanlis.");
    }

    if (message.kind == "end")
    {
        impl_->finished_ = true;
        std::cout << "Simulasyon bitti. Son tick: " << message.tick << std::endl;
        return false;
    }

    if (message.kind != "state")
    {
        throw std::runtime_error("Olcum mesaji bekleniyordu.");
    }

    impl_->awaiting_command_ = true;
    return true;
}

void Connection::send(const Message& message)   // komut, son alınan ölçümün adımına ait olmalı
{
    bool is_command = message.kind == "command";
    bool same_tick = message.tick == impl_->expected_tick_;
    if (!impl_->awaiting_command_ || !is_command || !same_tick)
    {
        throw std::runtime_error("Komut mevcut olcumun adimina ait olmali.");
    }

    impl_->write(message);
    impl_->awaiting_command_ = false;
    impl_->expected_tick_ += 1;
}
