#include <iostream>
#include <messages.pb.h>
#include <sstream>
#include "../myLibraries/Socket.h"

#define LISTEN_QUEUE 8

int main(int argc, char** argv) {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (argc != 2) {
        std::cerr << "Error: format is [" << argv[0] << "] [server port]" << std::endl;
        exit(1);
    }

    try{
        std::string serverPort = argv[1];
        int port = std::stoi(serverPort);

        //initialize server socket with port
        ServerSocket server{port, LISTEN_QUEUE, socketType::TCP};

        struct sockaddr_in client_address{};
        unsigned long client_address_len;
        //TODO put accept in a while loop
        Socket s = server.accept(&client_address, &client_address_len);
        //TODO create a thread pool (thread safe circular vector of threads)
        //TODO create atomic variable to make the threads blockable
        //TODO create thread (associated to the client socket) and insert it into thread queue
        //TODO inside the single thread, get socket, exchange data with client



    }
    catch (SocketException &e) {
        switch(e.getCode()){
            case socketError::create:
            case socketError::accept:
            default:
                std::cerr << e.what() << std::endl;
                return 1;
        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
