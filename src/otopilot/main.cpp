#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "control/sim.hpp"
#include "io/connection.hpp"

namespace
{
    struct Address                           // Python sunucusunun adresi; varsayılan scenario.yaml connection ile aynı
    {
        std::string host = "127.0.0.1";
        unsigned short port = 6000;
    };

    Address read_address(int argc, char* argv[])   // kullanım: main.exe [IPv4] [port]
    {
        if (argc > 3)
        {
            throw std::runtime_error("Kullanim: main.exe [IPv4] [port]");
        }

        Address address;
        if (argc > 1)
        {
            address.host = argv[1];
        }
        if (argc > 2)
        {
            const std::string text = argv[2];
            std::size_t used = 0;
            int value = 0;

            try
            {
                value = std::stoi(text, &used);
            }
            catch (const std::exception&)
            {
                used = 0;   // sayı değil: aşağıdaki kontrol yakalar
            }

            const bool whole_text_is_number = used == text.size();
            const bool in_range = value >= 1 && value <= 65535;
            if (!whole_text_is_number || !in_range)
            {
                throw std::runtime_error("Port 1..65535 araliginda olmali.");
            }
            address.port = static_cast<unsigned short>(value);
        }
        return address;
    }
}

int main(int argc, char* argv[])   // ana döngü: ölçümü al → Sim'e ver → komutu gönder
{
    try
    {
        const Address address = read_address(argc, argv);
        Connection connection(address.host, address.port);
        Sim sim;
        Message state;

        while (connection.receive(state))    // Python "end" gönderince false döner
        {
            const Message command = sim.step(state);
            connection.send(command);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Hata: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
