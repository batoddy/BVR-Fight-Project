#include <exception>
#include <iostream>
#include <string>

#include "control/sim.hpp"
#include "io/connection.hpp"

int main(int argc, char* argv[])   // kullanım: main.exe [IPv4] [port]; varsayılan scenario.yaml connection
{
    try
    {
        const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
        const int port = argc > 2 ? std::stoi(argv[2]) : 6000;

        Connection connection(host, static_cast<unsigned short>(port));
        Sim sim;
        sim.start(connection.receive_hello());   // uçaklar ve füze slotları

        Message state;
        while (connection.receive(state))        // Python "end" gönderince false
        {
            connection.send(sim.step(state));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Hata: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
