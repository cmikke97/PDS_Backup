//
// Created by michele on 28/07/2020.
//

#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include "Socket.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * TCP Socket implementation
 */

/**
 * TCP_Socket constructor
 *
 * @param sockfd
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket(int sockfd) : sockfd(sockfd) {   //assign to my sockfd the value provided
}

/**
 * TCP_Socket default constructor
 *
 * @throw SocketException in case the TCP_Socket cannot be created
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0); //create socket
    if(sockfd < 0)
        throw SocketException("Cannot create socket", socketError::create); //throw exception in case of errors
}

/**
 * TCP_Socket move constructor
 *
 * @param other TCP_Socket to move
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket(TCP_Socket &&other) noexcept : sockfd(other.sockfd) {    //assign to my sockfd the content of the other socket sockfd
    other.sockfd = 0;   //reset the other socket sockfd
}

/**
 * TCP_Socket operator= (with move)
 *
 * @param other TCP_Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket &TCP_Socket::operator=(TCP_Socket &&other) noexcept {
    if(sockfd != 0)     //if my sockfd has not been already closed (or never was opened) then close it
        close(sockfd);
    sockfd = other.sockfd;  //assign to my sockfd the value of the other socket
    other.sockfd = 0;   //reset the content of sockfd for the other socket
    return *this;
}

/**
 * connect TCP_Socket to TCP server
 *
 * @param addr of the TCP server to connect to
 * @param len of the struct addr
 *
 * @throw SocketException in case it cannot connect to remote TCP socket
 *
 * @author Michele Crepaldi s269551
 */
void TCP_Socket::connect(const std::string& addr, int port) {
    struct sockaddr_in address{};   //prepare sockaddr_in struct
    address.sin_family = AF_INET;
    inet_pton(AF_INET, addr.c_str(), &(address.sin_addr));
    address.sin_port = htons(port);
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)   //connect to server TCP socket
        throw SocketException("Cannot connect to remote socket", socketError::connect); //throw exception in case of errors
}

/**
 * read from connected TCP_Socket
 *
 * @param buffer where to put the read data
 * @param len of the data to read
 * @param options
 * @return number of bytes read
 *
 * @throw SocketException in case it cannot receive data from the TCP_socket
 *
 * @author Michele Crepaldi s269551
 */
ssize_t TCP_Socket::read(char *buffer, size_t len, int options) const {
    ssize_t res = recv(sockfd, buffer, len, options);   //receive len bytes from the socket and put it into buffer
    if(res < 0)
        throw SocketException("Cannot read from socket", socketError::read);    //if # of bytes read is < 0 throw exception
    return res;
}

/**
 * funtion to read a string from a TCP_Socket
 *
 * @return string read from TCP_socket
 *
 * @throw SocketException in case it cannot read data from the TCP_socket
 *
 * @author Michele Crepaldi s269551
*/
std::string TCP_Socket::recvString() const {
    std::string stringBuffer;
    uint32_t dataLength;
    ssize_t len = read(reinterpret_cast<char *>(&dataLength), sizeof(uint32_t), 0); // Receive the message length
    if(len < sizeof(uint32_t))
        throw SocketException("Cannot read from socket", socketError::read);    //if # of received bytes is less than expected throw exception

    dataLength = ntohl(dataLength); // Ensure host system byte order

    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    rcvBuf.resize(dataLength,0x00); // with the necessary size

    len = read(reinterpret_cast<char *>(&(rcvBuf[0])), dataLength, 0); // Receive the string data
    if(len < dataLength)
        throw SocketException("Cannot read from socket", socketError::read);    //if # of received bytes is less than expected throw exception

    stringBuffer.assign(reinterpret_cast<const char *>(&(rcvBuf[0])), rcvBuf.size()); // assign buffered data to a string
    return stringBuffer;
}

/**
 * write to connected TCP_Socket
 *
 * @param buffer where to get the data to write
 * @param len of the data to write
 * @param options
 * @return number of bytes written
 *
 * @throw SocketException in case it cannot write data to the TCP_socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t TCP_Socket::write(const char *buffer, size_t len, int options) const {
    ssize_t res = send(sockfd, buffer, len, options);   //send buffer through TCP socket
    if(res < 0)
        throw SocketException("Cannot write to socket", socketError::write);    //if # of sent bytes is < 0 throw exception
    return res;
}

/**
 * funtion to write a string to a TCP_Socket
 *
 * @param stringBuffer string to write to TCP_Socket
 *
 * @throw SocketException in case it cannot write data to the TCP_socket
 *
 * @author Michele Crepaldi s269551
*/
ssize_t TCP_Socket::sendString(std::string &stringBuffer) const {
    uint32_t dataLength = htonl(stringBuffer.size()); // Ensure network byte order when sending the data length

    ssize_t len = write(reinterpret_cast<const char *>(&dataLength), sizeof(uint32_t) , 0); // Send the data length
    if(len < sizeof(uint32_t))
        throw SocketException("Cannot write to socket", socketError::write);    //if # of sent bytes is less than expected throw exception
    return write(stringBuffer.c_str() ,stringBuffer.size() ,0); // Send the string data
}

/**
 * get the TCP_Socket file descriptor
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int TCP_Socket::getSockfd() const {
    return sockfd;
}

/**
 * get the machine MAC address from the TCP_socket
 *
 * @return MAC address of this machine's network card
 *
 * @throw SocketException in case of errors in getting the MAC address
 *
 * @author Michele Crepaldi s269551
 */
std::string TCP_Socket::getMAC() const {
    struct ifreq ifr{}; //prepare needed variables
    struct ifconf ifc{};
    char buf[1024];
    bool success = false;

    ifc.ifc_len = sizeof(buf);  //prepare ifc variable
    ifc.ifc_buf = buf;

    if (ioctl(sockfd, SIOCGIFCONF, &ifc) == -1)    //get the list of interface (transport layer) addresses associated to this socket (only for AF_INET (ipv4) addresses)
        throw SocketException("Error in getting MAC address", socketError::getMac); //error in getting the list

    struct ifreq* it = ifc.ifc_req; //define iterator and end element
    const struct ifreq* end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {   //for each interface address in the previously computed list do this
        strcpy(ifr.ifr_name, it->ifr_name); //copy the name of the interface as the ifreq struct name (subject of next instructions)

        if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0) {  //get flags for the interface

            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { //if the flag implies this is loopback interface then skip it

                if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) { //get the MAC address for this interface and stop the loop
                    success = true;
                    break;
                }
            }
        }
        else
            throw SocketException("Error in getting MAC address", socketError::getMac); //error in getting of the flags
    }

    if (!success)
        throw SocketException("Error in getting MAC address", socketError::getMac); //if no MAC address was found signal error

    auto *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data; //get mac address from struct ifreq

    std::stringstream mac;  //compose mac address string
    mac <<
        std::setw(2) <<
        std::hex << (int) hwaddr[0] << ":" <<
        std::hex << (int) hwaddr[1] << ":" <<
        std::hex << (int) hwaddr[2] << ":" <<
        std::hex << (int) hwaddr[3] << ":" <<
        std::hex << (int) hwaddr[4] << ":" <<
        std::hex << (int) hwaddr[5];

    return mac.str();   //return MAC address as string
}

/**
 * function used to manually close the connection
 *
 * @author Michele Crepaldi s269551
 */
void TCP_Socket::closeConnection() {
    if(sockfd != 0)     //if the socket is not already closed
        close(sockfd);  //close it
}

/**
 * TCP_Socket destructor
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::~TCP_Socket() {
    if(sockfd != 0)     //if the socket is not already closed
        close(sockfd);  //close it
}

/**
 * TCP_ServerSocket constructor
 *
 * @param port port to use
 *
 * @throw SocketException in case it cannot bind the port and create the TCP_socket
 *
 * @author Michele Crepaldi s269551
 */
TCP_ServerSocket::TCP_ServerSocket(int port) {
    struct sockaddr_in sockaddrIn{};    //prepare struct sockaddr_in
    sockaddrIn.sin_port = htons(port);
    sockaddrIn.sin_family = AF_INET;
    //sockaddrIn.sin_len = sizeof(sockaddrIn);
    sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY); //set address
    if(::bind(sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn)) != 0)    //bind the port and address to the socket
        throw SocketException("Cannot bind port", socketError::bind);   //throw exception if there are errors
    if(::listen(sockfd, 8) != 0)    //open the socket (set it to listen)
        throw SocketException("Cannot bind port", socketError::bind);   //throw exception if there are errors

    //extract ip address in readable form
    char address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sockaddrIn.sin_addr, address, sizeof(address));     //get address
    std::cout << "Server opened: available at [" << address << ":" << port << "]" << std::endl;
}

/**
 * accept incoming connections from TCP clients
 *
 * @param addr of the connected client
 * @param len of the struct addr
 * @return the newly created file descriptor (fd)
 *
 * @throw SocketException in case it cannot accept a socket
 *
 * @author Michele Crepaldi s269551
 */
SocketBridge* TCP_ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    int fd = ::accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), reinterpret_cast<socklen_t *>(len));    //accept connection from TCP client and get its fd (changing addr and len)
    if(fd < 0)
        throw SocketException("Cannot accept socket", socketError::accept);     //throw exception in case of errors
    return new TCP_Socket(fd);  //return a new TCP_Socket with the fd we just got
}

/**
 * TCP_ServerSocket destructor
 *
 * @author Michele Crepaldi s269551
 */
TCP_ServerSocket::~TCP_ServerSocket() {
    ::close(sockfd);    //close the socket
}

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * WTLS Socket implementation
 */

/**
 * TLS_Socket constructor
 *
 * @param sockfd
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket(int sockfd, WOLFSSL *ssl) : sock(new TCP_Socket(sockfd)), ssl(ssl) {   //assign to my sock a new TCP socket with the given fd and set ssl
}

/**
 * TLS_Socket default constructor
 *
 * @throw SocketException in case the TlS_Socket cannot be created or there are errors in the initialization of the wolfSSL context
 * or in the loading of the verifying CA
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket() {
    wolfSSL_Init();     //init wolfSSL library
    ctx = my::UniquePtr<WOLFSSL_CTX>(wolfSSL_CTX_new(wolfTLS_client_method()));     //crete wolfSSL context by using wolfTLS client method
    if (ctx.get() == nullptr)
        throw SocketException("Error in initializing wolfSSL context",socketError::create); //throw exception in case of errors

    if (wolfSSL_CTX_load_verify_locations(ctx.get(),CA_FILE_PATH,nullptr) != SSL_SUCCESS) { //load verify CA from the filesystem
        std::stringstream errorMsg;
        errorMsg << "Error loading " << CA_FILE_PATH << ", please check the file.";
        throw SocketException(errorMsg.str(),socketError::create);  //throw exception in case of errors
    }

    wolfSSL_CTX_set_verify(ctx.get(),SSL_VERIFY_PEER,nullptr);  //set the TLS client to verify the server certificate
    wolfSSL_CTX_set_verify_depth(ctx.get(),1);  //set the certificate verify depth into the CA

    sock = std::make_unique<TCP_Socket>();  //create TCP socket
}

/**
 * TLS_Socket move constructor
 *
 * @param other TLS_Socket to move
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket(TLS_Socket &&other) noexcept :
    sock(other.sock.release()),   //assign to my sock the other socket sock and reset it
    ctx(other.ctx.release()),   //assign to my ctx the context of the other socket (freeing it)
    ssl(other.ssl.release()) {    //assign to my ssl the other socket ssl (freeing it)
}

/**
 * TLS_Socket operator= (with move)
 *
 * @param other TLS_Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket& TLS_Socket::operator=(TLS_Socket &&other) noexcept {
    if(sock != nullptr) //if my sock has not been already closed (or never was opened)
        sock.reset();   //close it
    sock = std::unique_ptr<TCP_Socket>(other.sock.release());   //set the sock to be what the other's sock was

    if(ctx.get() != nullptr)    //if the context was set
        ctx.reset();    //reset (free) it
    ctx = my::UniquePtr<WOLFSSL_CTX>(other.ctx.release());  //set the context to be what the other's context was

    if(ssl.get() != nullptr)    //if the ssl object was set
        ssl.reset();    //reset (free) it
    ssl = my::UniquePtr<WOLFSSL>(other.ssl.release()); //set the ssl object to be what the other's ssl was
    return *this;
}

/**
 * connect TLS_Socket to TLS server
 *
 * @param addr of the TLS server to connect to
 * @param len of the struct addr
 *
 * @throw SocketException in case it cannot connect to remote TLS socket or if it cannot create the wolfSSL ssl
 *
 * @author Michele Crepaldi s269551
 */
void TLS_Socket::connect(const std::string& addr, int port) {
    sock->connect(addr, port);

    ssl = my::UniquePtr<WOLFSSL>(wolfSSL_new(ctx.get()));   //create a new wolfSSL object from the context
    if (ssl.get() == nullptr)
        throw SocketException("Error in creating wolfSSL ssl", socketError::connect); //throw exception in case of errors

    if(wolfSSL_set_fd(ssl.get(), sock->getSockfd()) != SSL_SUCCESS) //associate the socket file descritor to the wolfSSL object
        throw SocketException("Cannot set fd to wolfSSL ssl", socketError::connect); //throw exception in case of errors
}

/**
 * read from connected TLS_Socket
 *
 * @param buffer where to put the read data
 * @param len of the data to read
 * @return number of bytes read
 *
 * @throw SocketException in case it cannot receive data from the TLS_socket
 *
 * @author Michele Crepaldi s269551
 */
ssize_t TLS_Socket::read(char *buffer, size_t len) const {
    ssize_t res = wolfSSL_read(ssl.get(), buffer, len);   //receive len bytes from the socket and put it into buffer
    if(res < 0) {
        char errorString[80];
        int err = wolfSSL_get_error(ssl.get(), 0);  //get error code
        wolfSSL_ERR_error_string(err, errorString); //get string from error code
        std::stringstream errMsg;
        errMsg << "Cannot read from socket. Error: " << errorString;
        throw SocketException(errMsg.str(), socketError::read);    //if # of bytes read is < 0 throw exception
    }
    return res;
}

/**
 * funtion to read a string from a TLS_Socket
 *
 * @return string read from TLS_socket
 *
 * @throw SocketException in case it cannot read data from the TLS_socket or data read is less than expected
 *
 * @author Michele Crepaldi s269551
*/
std::string TLS_Socket::recvString() const {
    std::string stringBuffer;
    uint32_t dataLength;
    ssize_t len = read(reinterpret_cast<char *>(&dataLength), sizeof(uint32_t)); // Receive the message length
    if(len < sizeof(uint32_t))
        throw SocketException("Read from socket error, read bytes are less than expected", socketError::read);    //if # of received bytes is less than expected throw exception

    dataLength = ntohl(dataLength); // Ensure host system byte order

    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    rcvBuf.resize(dataLength,0x00); // with the necessary size

    len = read(reinterpret_cast<char *>(&(rcvBuf[0])), dataLength); // Receive the string data
    if(len < dataLength)
        throw SocketException("Read from socket error, read bytes are less than expected", socketError::read);    //if # of received bytes is less than expected throw exception

    stringBuffer.assign(reinterpret_cast<const char *>(&(rcvBuf[0])), rcvBuf.size()); // assign buffered data to a string
    return stringBuffer;
}

/**
 * write to connected TLS_Socket
 *
 * @param buffer where to get the data to write
 * @param len of the data to write
 * @return number of bytes written
 *
 * @throw SocketException in case it cannot write data to the TLS_socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t TLS_Socket::write(const char *buffer, size_t len) const {
    ssize_t res = wolfSSL_write(ssl.get(), buffer, len);   //send buffer through TLS socket
    if(res < 0) {
        char errorString[80];
        int err = wolfSSL_get_error(ssl.get(), 0);  //get error code
        wolfSSL_ERR_error_string(err, errorString); //get error string from error code
        std::stringstream errMsg;
        errMsg << "Cannot write to socket. Error: " << errorString;
        throw SocketException(errMsg.str(),socketError::write);    //if # of sent bytes is < 0 throw exception
    }
    return res;
}

/**
 * funtion to write a string to a TLS_Socket
 *
 * @param stringBuffer string to write to TLS_Socket
 *
 * @throw SocketException in case it cannot write data to the TLS_socket or if data written is less than expected
 *
 * @author Michele Crepaldi s269551
*/
ssize_t TLS_Socket::sendString(std::string &stringBuffer) const {
    uint32_t dataLength = htonl(stringBuffer.size()); // Ensure network byte order when sending the data length

    ssize_t len = write(reinterpret_cast<const char *>(&dataLength), sizeof(uint32_t)); // Send the data length
    if(len < sizeof(uint32_t))
        throw SocketException("Write to socket error, sent bytes are less than expected", socketError::write);    //if # of sent bytes is less than expected throw exception
    return write(stringBuffer.c_str() ,stringBuffer.size()); // Send the string data
}

/**
 * get the TLS_Socket file descriptor
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int TLS_Socket::getSockfd() const {
    return sock->getSockfd();
}

/**
 * get the machine MAC address from the TLS_socket
 *
 * @return MAC address of this machine's network card
 *
 * @throw SocketException in case of errors in getting the MAC address
 *
 * @author Michele Crepaldi s269551
 */
std::string TLS_Socket::getMAC() const {
    return sock->getMAC();
}

/**
 * function used to manually close the TLS connection
 *
 * @author Michele Crepaldi s269551
 */
void TLS_Socket::closeConnection() {
    sock.reset();   //close the socket
}

/**
 * TLS_Socket destructor
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::~TLS_Socket() {
    //the TCP socket will already be closed by the unique pointer
    wolfSSL_Cleanup();  //cleanup the wolfSSL library
}

/**
 * TLS_ServerSocket constructor
 *
 * @param port port to use
 *
 * @throw SocketException in case it cannot bind the port and create the TLS_socket or in case it cannot create the wolfSSL context
 * or cannot load certificate or private key files (or they are not valid)
 *
 * @author Michele Crepaldi s269551
 */
TLS_ServerSocket::TLS_ServerSocket(int port) {
    ctx = my::UniquePtr<WOLFSSL_CTX>(wolfSSL_CTX_new(wolfTLS_server_method())); //create a new wolfSSL context using the wolfTLS server method
    if (ctx.get() == nullptr)
        throw SocketException("Cannot create server wolfSSL context", socketError::create);   //throw exception if there are errors

    wolfSSL_CTX_SetMinVersion(ctx.get(), WOLFSSL_TLSV1_2);  //set minimum TLS version supported by the server

    if (wolfSSL_CTX_use_certificate_file(ctx.get(),CERTIFICATE_PATH, SSL_FILETYPE_PEM) != SSL_SUCCESS) {    //load the server certificate
        std::stringstream errorMsg;
        errorMsg << "Error loading " << CERTIFICATE_PATH << ", please check the file.";
        throw SocketException(errorMsg.str(), socketError::create);   //throw exception if there are errors
    }

    if (wolfSSL_CTX_use_PrivateKey_file(ctx.get(),PRIVATEKEY_PATH, SSL_FILETYPE_PEM) != SSL_SUCCESS) {  //load the server private key
        std::stringstream errorMsg;
        errorMsg << "Error loading " << PRIVATEKEY_PATH << ", please check the file.";
        throw SocketException(errorMsg.str(), socketError::create);   //throw exception if there are errors
    }

    if (!wolfSSL_CTX_check_private_key(ctx.get()))  //Check if the server certificate and private-key match
        throw SocketException("Private key does not match the certificate public key", socketError::create);    //throw exception in case they don't match

    wolfSSL_CTX_set_verify(ctx.get(), SSL_VERIFY_NONE, nullptr);    //set the server to not ask the client for a certificate (no client authentication supported)

    serverSock = std::make_unique<TCP_ServerSocket>(port);  //create new TCP server socket
}

/**
 * accept incoming connections from TLS clients
 *
 * @param addr of the connected client
 * @param len of the struct addr
 * @return the newly created file descriptor (fd)
 *
 * @throw SocketException in case it cannot accept a socket or cannot create a new wolfSSL socket
 *
 * @author Michele Crepaldi s269551
 */
SocketBridge* TLS_ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    auto s = serverSock->accept(addr, len); //accept tcp socket

    auto ssl = my::UniquePtr<WOLFSSL>(wolfSSL_new(ctx.get()));  //create new wolfSSL from context
    if (ssl == nullptr)
        throw SocketException("Cannot create new wolfSSL socket", socketError::accept);     //throw exception in case of errors

    int err = wolfSSL_set_fd(ssl.get(), s->getSockfd());  //associate the socket file descriptor to the newly created wolfSSL
    if(err != SSL_SUCCESS)
        throw SocketException("Cannot set fd to wolfSSL socket", socketError::accept);     //throw exception in case of errors
    return new TLS_Socket(s->getSockfd(), ssl.release());  //return a new TLS_Socket with the fd (and wolfSSL) we just got
}

/**
 * TLS_ServerSocket destructor
 *
 * @author Michele Crepaldi s269551
 */
TLS_ServerSocket::~TLS_ServerSocket() {
    //the TCP server socket will already be closed by the unique pointer
    wolfSSL_Cleanup();  //cleanup the wolfSSL library
}

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * socket class
 */

/**
 * Socket constructor
 *
 * @param SocketBridge to create a new socket from
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(SocketBridge *sb) : socket(std::unique_ptr<SocketBridge>(sb)) {
}

/**
 * Socket default constructor
 *
 * @param type type of the socket to be created; between socketType::TCP and socketType::TLS
 *
 * @throw SocketException in case the TlS_Socket cannot be created [or there are errors in the initialization of the wolfSSL context
 * or in the loading of the verifying CA (if TLS)]
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(socketType type) {
    switch(type){
        case socketType::TCP:
            socket = std::make_unique<TCP_Socket>();
            break;
        case socketType::TLS:
            socket = std::make_unique<TLS_Socket>();
            break;
        default:
            throw SocketException("Undefined socket type",socketError::create);
    }
}

/**
 * Socket move constructor
 *
 * @param other Socket to move
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(Socket &&other) noexcept : socket(other.socket.release()) {
}

/**
 * Socket operator= (with move)
 *
 * @param other Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
Socket &Socket::operator=(Socket &&other) noexcept {
    if(socket != nullptr)
        socket.reset();
    socket = std::unique_ptr<SocketBridge>(other.socket.release());
    return *this;
}

/**
 * connect Socket to server
 *
 * @param addr of the server to connect to
 * @param len of the struct addr
 *
 * @throw SocketException in case it cannot connect to remote socket [or if it cannot create the wolfSSL ssl (if TLS)]
 *
 * @author Michele Crepaldi s269551
 */
void Socket::connect(const std::string &addr, int port) {
    socket->connect(addr, port);
}

/**
 * funtion to read a string from a Socket
 *
 * @return string read from socket
 *
 * @throw SocketException in case it cannot read data from the socket or data read is less than expected
 *
 * @author Michele Crepaldi s269551
*/
std::string Socket::recvString() const {
    return socket->recvString();
}

/**
 * funtion to write a string to a Socket
 *
 * @param stringBuffer string to write to Socket
 *
 * @throw SocketException in case it cannot write data to the socket or if data written is less than expected
 *
 * @author Michele Crepaldi s269551
*/
ssize_t Socket::sendString(std::string &stringBuffer) const {
    return socket->sendString(stringBuffer);
}

/**
 * get the Socket file descriptor
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int Socket::getSockfd() const {
    return socket->getSockfd();
}

/**
 * get the machine MAC address from the socket
 *
 * @return MAC address of this machine's network card
 *
 * @throw SocketException in case of errors in getting the MAC address
 *
 * @author Michele Crepaldi s269551
 */
std::string Socket::getMAC() {
    return socket->getMAC();
}

/**
 * function used to manually close the connection
 *
 * @author Michele Crepaldi s269551
 */
void Socket::closeConnection() {
    socket->closeConnection();
}

/**
 * ServerSocket constructor
 *
 * @param port port to use
 *
 * @throw SocketException in case it cannot bind the port and create the socket [or in case it cannot create the wolfSSL context
 * or cannot load certificate or private key files (or they are not valid) (if TLS)]
 *
 * @author Michele Crepaldi s269551
 */
ServerSocket::ServerSocket(int port, socketType type) : Socket(type) {
    switch(type){
        case socketType::TCP:
            serverSocket = std::make_unique<TCP_ServerSocket>(port);
            break;
        case socketType::TLS:
            serverSocket = std::make_unique<TLS_ServerSocket>(port);
            break;
        default:
            throw SocketException("Undefined server socket type",socketError::create);
    }
}

/**
 * accept incoming connections from clients
 *
 * @param addr of the connected client
 * @param len of the struct addr
 * @return the newly created file descriptor (fd)
 *
 * @throw SocketException in case it cannot accept a socket [or cannot create a new wolfSSL socket (if TLS)]
 *
 * @author Michele Crepaldi s269551
 */
Socket ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    return Socket(serverSocket->accept(addr, len));
}
