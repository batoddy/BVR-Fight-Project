#include <iostream>
#include <string>
#include "tcp_connection.hpp"
#include "controller.hpp"

int main(int argc, char* argv[]) {
    try {
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

        TCPConnection connection(host, port);
        Controller controller;
        std::uint64_t expected_tick = 0;
        std::cout << "Python'a baglandi: " << host << ':' << port << '\n';

        while (true) {
            // 1. Ölçümün tamamını mesaj nesnesine al.
            Message state = connection.receive();
            if (state.tick != expected_tick)
                throw std::runtime_error("Olcum adim sirasi yanlis.");
            if (state.kind == "end") {
                std::cout << "Simulasyon bitti. Son tick: " << state.tick << '\n';
                break;
            }
            if (state.kind != "state")
                throw std::runtime_error("Olcum mesaji bekleniyordu.");

            // 2. Kendi hesabını yap; 3. komutu aynı bağlantıda geri gönder.
            Message command = controller.calculate(state);
            connection.send(command);
            ++expected_tick;

            // Terminali her fizik adımında doldurma; yaklaşık 10 sim saniyesinde bir.
            if (state.tick % 120 == 0)
                std::cout << "Tick: " << state.tick
                          << " | sim time: " << state.data.at("sim_time_s") << " s\n";
        }
    }
    catch (const std::exception& error) {
        std::cerr << "Hata: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
