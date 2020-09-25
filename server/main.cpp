#include <iostream>
#include <messages.pb.h>
#include <sstream>
#include "../myLibraries/Socket.h"

int main(int argc, char** argv) {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (argc != 3) {
        std::cerr << "Error: format is [" << argv[0] << "] [backup directory] [server port]" << std::endl;
        exit(1);
    }

    try{
        //initialize server socket with port
        ServerSocket socket(std::stoi(argv[2]));

        struct sockaddr_in client_address{};
        unsigned int client_address_len;
        Socket clientSocket = socket.accept(&client_address, &client_address_len);

        //TODO vai avanti

    }catch(std::exception &e){

    }


    return 0;
}
