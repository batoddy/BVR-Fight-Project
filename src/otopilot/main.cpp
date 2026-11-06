#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "control/sim.hpp"
#include "io/connection.hpp"

namespace
{
    struct Address                           // Python sunucusunun adresi; varsayılan scenario.yaml connection ile aynı
    {
        std::string host = "127.0.0.1";
        unsigned short port = 6000;
        bool maneuver_test = false;
    };

    Address read_address(int argc, char* argv[])   // kullanım: main.exe [--maneuver-test] [IPv4] [port]
    {
        Address address;
        std::vector<std::string> positional;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--maneuver-test") { address.maneuver_test = true; }
            else if (argument.rfind("--", 0) == 0)
            {
                throw std::runtime_error("Bilinmeyen secenek: " + argument);
            }
            else { positional.push_back(argument); }
        }
        if (positional.size() > 2)
        {
            throw std::runtime_error("Kullanim: main.exe [--maneuver-test] [IPv4] [port]");
        }

        if (!positional.empty())
        {
            address.host = positional[0];
        }
        if (positional.size() > 1)
        {
            const std::string text = positional[1];
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
        Sim sim(address.maneuver_test);
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
