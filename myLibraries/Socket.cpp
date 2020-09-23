//
// Created by michele on 28/07/2020.
//

#include "Socket.h"

/**
 * Socket constructor
 *
 * @param sockfd
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(int sockfd) : sockfd(sockfd) {
}

/**
 * Socket default constructor
 *
 * @throw runtime error in case the socket cannot be created
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        throw std::runtime_error("Cannot create socket");
}

/**
 * Socket destructor
 *
 * @author Michele Crepaldi s269551
 */
Socket::~Socket() {
    if(sockfd != 0)
        close(sockfd);
}

/**
 * Socket move constructor
 *
 * @param other socket to move
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(Socket &&other) noexcept : sockfd(other.sockfd) {
    other.sockfd = 0;
}

/**
 * Socket operator= (with move)
 *
 * @param other socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
Socket &Socket::operator=(Socket &&other) noexcept {
    if(sockfd != 0)
        close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    return *this;
}

/**
 * read from connected Socket
 *
 * @param buffer where to put the read data
 * @param len of the data to read
 * @param options
 * @return number of bytes read
 *
 * @throw runtime error in case it cannot receive data from socket
 *
 * @author Michele Crepaldi s269551
 */
ssize_t Socket::read(char *buffer, size_t len, int options) {
    ssize_t res = recv(sockfd, buffer, len, options);
    if(res < 0)
        throw std::runtime_error("Cannot receive from socket");
    return res;
}

/**
 * write to connected Socket
 *
 * @param buffer where to get the data to write
 * @param len of the data to write
 * @param options
 * @return number of bytes written
 *
 * @throw runtime error in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t Socket::write(const char *buffer, size_t len, int options) {
    ssize_t res = send(sockfd, buffer, len, options);
    if(res < 0)
        throw std::runtime_error("Cannot write to socket");
    return res;
}

/**
 * connect Socket to server
 *
 * @param addr of the server to connect to
 * @param len of the struct addr
 *
 * @throw runtime error in case it cannot connect to remote socket
 *
 * @author Michele Crepaldi s269551
 */
void Socket::connect(struct sockaddr_in *addr, unsigned int len) {
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(addr), len) != 0)
        throw std::runtime_error("Cannot connect to remote socket");
}

/**
 * get the socket file descriptor
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int Socket::getSockfd() {
    return sockfd;
}

struct sockaddr_in Socket::composeAddress(const std::string& addr, const std::string& port) {
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, addr.c_str(), &(address.sin_addr));
    address.sin_port = htons(stoi(port));
    return address;
}

/**
 * ServerSocket constructor
 *
 * @param port
 *
 * @throw runtime error in case it cannot bind the port and create the socket
 *
 * @author Michele Crepaldi s269551
 */
ServerSocket::ServerSocket(int port) {
    struct sockaddr_in sockaddrIn{};
    sockaddrIn.sin_port = htons(port);
    sockaddrIn.sin_family = AF_INET;
    //sockaddrIn.sin_len = sizeof(sockaddrIn);
    sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY);
    if(::bind(sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn)) != 0)
        throw std::runtime_error("Cannot bind port");
    if(::listen(sockfd, 8) != 0)
        throw std::runtime_error("Cannot bind port");
}

/**
 * accept incoming connections from clients
 *
 * @param addr of the connected client
 * @param len of the struct addr
 * @return the newly created file descriptor (fd)
 *
 * @throw runtime error in case it cannot accept a socket
 *
 * @author Michele Crepaldi s269551
 */
Socket ServerSocket::accept(struct sockaddr_in *addr, unsigned int *len) {
    int fd = ::accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), reinterpret_cast<socklen_t *>(len));
    if(fd < 0)
        throw std::runtime_error("Cannot accept socket");
    return Socket(fd);
}
