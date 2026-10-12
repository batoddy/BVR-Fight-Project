#include <iostream>
#include "connection.hpp"
#include "controller.hpp"

// Ana döngü: ölçümü al -> hesabı çağır -> komutu gönder.
// TCP ayrıntıları connection, karar kodu controller içindedir.
int main(int argc, char* argv[])
{
    try
    {
        Connection connection(argc, argv);
        Controller controller;
        Message state;

        while (connection.receive(state))
        {
            Message command = controller.calculate(state);
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
