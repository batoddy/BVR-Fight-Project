#include <iostream>
#include "control/sim.hpp"
#include "io/connection.hpp"

// Ana döngü: ölçümü al -> Sim'e ver -> komutu gönder.
// TCP ayrıntıları Connection, adımın yönetimi Sim içindedir.
int main(int argc, char* argv[])
{
    try
    {
        Connection connection(argc, argv);   // Python sunucusuna bağlan (varsayılan 127.0.0.1:6000)
        Sim sim;
        Message state;

        while (connection.receive(state))    // Python "end" gönderince false döner
        {
            Message command = sim.step(state);
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
