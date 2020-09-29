//
// Created by michele on 28/07/2020.
//

#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>

#include <sys/ioctl.h>
#include <net/if.h>

#define MAXBUFFSIZE 1024

enum class socketError{create, bind, accept, read, write, connect, getMac};

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
    void closeConnection() const;
    ssize_t read(char *buffer, size_t len, int options) const;
    std::string recvString(int options) const;
    ssize_t write(const char *buffer, size_t len, int options) const;
    ssize_t sendString(std::string &stringBuffer, int options) const;
    static struct sockaddr_in composeAddress(const std::string& addr, const std::string& port);
    void connect(struct sockaddr_in *addr, unsigned int len) const;
    int getSockfd() const;
    std::string getMAC() const;
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

/**
 * exceptions for the socket class
 *
 * @author Michele Crepaldi s269551
 */
class SocketException : public std::runtime_error {
    socketError code;
public:

    /**
     * socket exception constructor
     *
     * @param msg the error message
     *
     * @author Michele Crepaldi s269551
     */
    SocketException(const std::string& msg, socketError code):
            std::runtime_error(msg), code(code){
    }

    /**
     * socket exception destructor.
     *
     * @author Michele Crepaldi s269551
     */
    ~SocketException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return error code
     *
     * @author Michele Crepaldi s269551
     */
    socketError getCode() const noexcept{
        return code;
    }
};

#endif //CLIENT_SOCKET_H
