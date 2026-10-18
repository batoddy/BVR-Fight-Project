#include <iostream>
#include "autopilot.hpp"
#include "connection.hpp"

// Ana döngü: ölçümü al -> otopilota ver -> komutu gönder.
// TCP ayrıntıları Connection, uçuş mantığı Autopilot içindedir.
int main(int argc, char* argv[])
{
    try
    {
        Connection connection(argc, argv);   // Python sunucusuna bağlan (varsayılan 127.0.0.1:6000)
        Autopilot autopilot;
        Message state;

        while (connection.receive(state))    // Python "end" gönderince false döner
        {
            Message command = autopilot.step(state);
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
