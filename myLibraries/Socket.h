//
// Created by michele on 28/07/2020.
//

#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>

#define MAXBUFFSIZE 1024

/**
 * Socket class
 *
 * @author Michele Crepaldi s269551
 */
class Socket {
    int sockfd;

    explicit Socket(int sockfd);
    friend class ServerSocket;

public:
    Socket(const Socket &) = delete;
    Socket& operator=(const Socket &) = delete;

    Socket();
    ~Socket();
    Socket(Socket &&other) noexcept;
    Socket& operator=(Socket &&other) noexcept;
    ssize_t read(char *buffer, size_t len, int options);
    std::string recvString(int options);
    void recvFile(const std::filesystem::path& path, uintmax_t expectedFileSize, int options);
    ssize_t write(const char *buffer, size_t len, int options);
    void sendString(std::string &stringBuffer, int options);
    void sendFile(const std::filesystem::path& path, int options);
    static struct sockaddr_in composeAddress(const std::string& addr, const std::string& port);
    void connect(struct sockaddr_in *addr, unsigned int len);
    int getSockfd();
};

/**
 * Server Socket class
 *
 * @author Michele Crepaldi s269551
 */
class ServerSocket: private Socket{
public:
    explicit ServerSocket(int port);
    Socket accept(struct sockaddr_in* addr, unsigned int* len);
};

#endif //CLIENT_SOCKET_H
