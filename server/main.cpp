#include <iostream>
#include <messages.pb.h>
#include <sstream>
#include <thread>
#include "../myLibraries/Socket.h"
#include "../myLibraries/TSCircular_vector.h"
#include "Thread_guard.h"

#define VERSION 1
#define CONFIG_FILE_PATH "./config.txt"

#define LISTEN_QUEUE 8
#define N_THREADS 8
#define SOCKET_QUEUE_SIZE 10
//TODO put these constants into the config object

void single_server(TSCircular_vector<Socket> &, std::atomic<bool> &);

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

        //TODO get config
        //TODO (get db reference)
        //TODO load db

        ServerSocket server{port, LISTEN_QUEUE, socketType::TCP};   //initialize server socket with port
        TSCircular_vector<Socket> sockets{SOCKET_QUEUE_SIZE};

        std::atomic<bool> server_thread_stop = false;   //atomic boolean to force the server threads to stop
        std::vector<std::thread> threads;

        for(int i=0; i<N_THREADS; i++)  //create N_THREADS threads and start them
            threads.emplace_back(single_server, std::ref(sockets), std::ref(server_thread_stop));

        Thread_guard td{threads, server_thread_stop};

        struct sockaddr_in addr{};
        unsigned long addr_len;

        while(true){
            //alternative: wait here for free space in circular vector and then accept and push

            Socket s = server.accept(&addr, &addr_len); //accept a new client socket
            sockets.push(std::move(s)); //push the socket into the socket queue

            auto clientAddress = inet_ntoa(addr.sin_addr);  //obtain ip address of newly connected client
            std::cout << "New connection from " << clientAddress << ":" << addr.sin_port << std::endl;  //print ip address and port of the newly connected client
        }
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
}

void single_server(TSCircular_vector<Socket> &sockets, std::atomic<bool> &thread_stop){
    while(!thread_stop.load()){ //loop until we are told to stop
        Socket sock = sockets.get();    //get the first socket in the socket queue (removing it from the queue); if no element is present then passively wait

        //TODO authenticate client

        //TODO while loop (communication)
            //TODO select -> read/write(only for async)
                /*TODO if there is something to read pass it to the protocol manager (pm);
                 * (sync alternative): the protocol manager reads the message, interprets it and based on the type
                 * it performs the related actions; when all actions are done it sends the response to the client
                 * (async alternative)L the protocol manager reads the message and inputs it in a list of incoming
                 * messages; a concurrent thread gets messages from the list of incoming messages, interprets them
                 * and based on the types it performs the related actions; when all actions related to a single message
                 * are done it inputs the related response message in the list of outgoing messages; so now if there
                 * are messages in the list of outgoing messages then send one of them (for every select cycle)*/

        //TODO implement protocol manager
    }
}