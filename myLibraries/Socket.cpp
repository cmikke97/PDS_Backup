//
// Created by Michele Crepaldi s269551 on 28/07/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include "Socket.h"

#include <vector>
#include <filesystem>
#include <iostream>
#include <net/if.h> //needed by linux
#include <fstream>
#include <cstdio>
#include <cstring>
#include <unistd.h>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * SocketBridge interface
 */

SocketBridge::~SocketBridge()= default;
ServerSocketBridge::~ServerSocketBridge()= default;


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * TCP Socket implementation
 */

/**
 * TCP_Socket class constructor
 *
 * @param sockfd socket file descriptor of the already opened socket
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket(int sockfd) : _sockfd(sockfd) {
}

/**
 * TCP_Socket class empty constructor
 *
 * @throws SocketException:
 *  <b>create</b> if the TCP_Socket could not be created
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket() {
    _sockfd = ::socket(AF_INET, SOCK_STREAM, 0); //create socket
    if(_sockfd < 0)
        throw SocketException("Cannot create socket", SocketError::create);
}

/**
 * TCP_Socket class move constructor
 *
 * @param other TCP_Socket to move
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::TCP_Socket(TCP_Socket &&other) noexcept :
    _sockfd(other._sockfd) {    //assign to my sockfd the content of the other socket sockfd

    other._sockfd = 0;  //reset the other socket sockfd
}

/**
 * TCP_Socket operator= (with move) override
 *
 * @param other TCP_Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket& TCP_Socket::operator=(TCP_Socket &&other) noexcept {
    if(_sockfd != 0)    //if my sockfd has not been already closed (or never was opened) then close it
        close(_sockfd);
    _sockfd = other._sockfd;    //assign to my sockfd the value of the other socket
    other._sockfd = 0;          //reset the content of sockfd for the other socket
    return *this;
}

/**
 * TCP_Socket connect method
 *
 * @param addr address of the TCP server to connect to
 * @param len length of the struct addr
 *
 * @throws SocketException:
 *  <b>connect</b> if it could not connect to remote TCP socket
 *
 * @author Michele Crepaldi s269551
 */
void TCP_Socket::connect(const std::string &addr, unsigned int port) {
    struct sockaddr_in address{};   //prepare sockaddr_in struct
    address.sin_family = AF_INET;   //set address family
    inet_pton(AF_INET, addr.c_str(), &(address.sin_addr));  //convert to network address
    address.sin_port = htons(port); //set port

    //connect to server TCP socket
    if(::connect(_sockfd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)
        throw SocketException("Cannot connect to remote socket", SocketError::connect);
}

/**
 * TCP_Socket read method
 *
 * @param buffer buffer where to put the read data
 * @param len length of the data to read
 * @param options
 * @return number of bytes read
 *
 * @throws SocketException:
 *  <b>read</b> if it could not receive data from the TCP_socket
 * @throws SocketException:
 *  <b>closed</b> if the socket is closed
 *
 * @author Michele Crepaldi s269551
 */
ssize_t TCP_Socket::read(char *buffer, size_t len, int options) const {
    char *ptr = buffer; //pointer to buffer
    int64_t numRec;     //number of bytes received
    int64_t rec = 0;        //total amount of bytes received

    while(len > 0){
        //receive len bytes from the socket and put it into the buffer at the position pointed by ptr
        numRec = recv(_sockfd, ptr, len, options);
        if(numRec < 0)
            //if # of bytes read is < 0 throw exception
            throw SocketException("Cannot read from socket", SocketError::read);

        if(numRec == 0)
            //if # of bytes read is == 0 the socket was closed from the other party, so throw exception
            throw SocketException("Socket closed", SocketError::closed);

        //update variables
        ptr += numRec;
        len -= numRec;
        rec += numRec;
    }
    return rec;
}

/**
 * TCP_Socket receive string method
 *
 * @return string read from TCP_socket
 *
 * @throws SocketException:
 *  <b>read</b> if it could not read data from the TCP_socket or data is less than expected
 *
 * @author Michele Crepaldi s269551
*/
std::string TCP_Socket::recvString() const {
    std::string stringBuffer;   //string buffer
    uint32_t dataLength;        //data length

    //receive first the message length

    int64_t len = read(reinterpret_cast<char *>(&dataLength), sizeof(uint32_t), 0); //bytes read

    //if # of received bytes is less than expected throw exception
    if(len < sizeof(uint32_t))
        throw SocketException("Read from socket error, read bytes are less than expected", SocketError::read);

    dataLength = ntohl(dataLength); //ensure host system byte order

    std::vector<uint8_t> rcvBuf;        //receive buffer
    rcvBuf.resize(dataLength,0x00);  //initialize receive buffer with the necessary size

    len = read(reinterpret_cast<char *>(&(rcvBuf[0])), dataLength, 0);  //receive the data

    //if # of received bytes is less than expected throw exception
    if(len < dataLength)
        throw SocketException("Read from socket error, read bytes are less than expected", SocketError::read);

    //assign buffered data to a string
    stringBuffer.assign(reinterpret_cast<const char *>(&(rcvBuf[0])), rcvBuf.size());
    return stringBuffer;
}

/**
 * TCP_Socket write method
 *
 * @param buffer buffer where to get the data to write
 * @param len length of the data to write
 * @param options
 * @return number of bytes written
 *
 * @throws SocketException:
 *  <b>write</b> if it could not write data to the TCP_socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t TCP_Socket::write(const char *buffer, size_t len, int options) const {
    const char *ptr = (const char*)buffer;  //pointer to buffer
    int64_t numSent;     //number of bytes written
    int64_t sent = 0;        //total amount of bytes written

    while(len > 0){
        //send len bytes from the buffer at the position pointed by ptr to socket
        numSent = send(_sockfd, ptr, len, options);

        //if # of sent bytes is < 0 throw exception
        if(numSent < 0)
            throw SocketException("Cannot write to socket", SocketError::write);

        //update variables
        ptr += numSent;
        len -= numSent;
        sent += numSent;
    }
    return sent;
}

/**
 * TCP_Socket send string method
 *
 * @param stringBuffer string to write to TCP_Socket
 *
 * @throws SocketException:
 *  <b>write</b> if it could not write data to the TCP_socket or if data written is less than expected
 *
 * @author Michele Crepaldi s269551
*/
ssize_t TCP_Socket::sendString(std::string &stringBuffer) const {
    //ensure network byte order when sending the data length

    uint32_t dataLength = htonl(stringBuffer.size());   //data to send size

    //send first the data length
    ssize_t len = write(reinterpret_cast<const char *>(&dataLength), sizeof(uint32_t) , 0);

    //if # of sent bytes is less than expected throw exception
    if(len < sizeof(uint32_t))
        throw SocketException("Write to socket error, sent bytes are less than expected", SocketError::write);

    //send the string data;
    return write(stringBuffer.data() ,stringBuffer.size() ,0);
}

/**
 * TCP_Socket socket file descriptor getter method
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int TCP_Socket::getSockfd() const {
    return _sockfd;
}

/**
 * TCP_socket MAC address getter method
 *
 * @return MAC address of this machine's network card
 *
 * @throws SocketException:
 *  <b>getMac</b> if it could not get the MAC address
 *
 * @author Michele Crepaldi s269551
 */
std::string TCP_Socket::getMAC() const {
    struct ifreq ifr{};     //ifreq struct variable
    struct ifconf ifc{};    //ifconf struct variable
    char buf[1024];
    sockaddr_in *ip_address;    //sockaddr_in variable

    ifc.ifc_len = sizeof(buf);  //prepare ifc variable
    ifc.ifc_buf = buf;

    //get the list of interface (transport layer) addresses associated to this socket (only for AF_INET (ipv4) addresses)
    if (ioctl(_sockfd, SIOCGIFCONF, &ifc) == -1)
        throw SocketException("Error in getting MAC address", SocketError::getMac);

    struct ifreq* it = ifc.ifc_req;     //iterator for the interface list
    const struct ifreq* end = it + (ifc.ifc_len / sizeof(struct ifreq));    //end element of the list

    bool success = false;
    for (; it != end; ++it) {   //for each interface address in the previously computed list do this

        //copy the name of the interface as the ifreq struct name (subject of next instructions)
        strcpy(ifr.ifr_name, it->ifr_name);

        if (ioctl(_sockfd, SIOCGIFFLAGS, &ifr) == 0) {  //get flags for the interface

            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { //if the flag implies this is loopback interface then skip it

                if(ioctl(_sockfd, SIOCGIFADDR, &ifr) == 0){  //get the ip address associated to this interface

                    //get the ip address from the ifr element (just updated)
                    ip_address = reinterpret_cast<sockaddr_in *>(&ifr.ifr_addr);

                    char buf2[INET_ADDRSTRLEN];

                    //convert the address to human readable form
                    if (inet_ntop(AF_INET, &ip_address->sin_addr, buf2, INET_ADDRSTRLEN) == nullptr)
                        throw SocketException("Could not inet_ntop", SocketError::getMac);

                    //compare the ip address associated to this interface with the actual socket ip address
                    if(std::string(buf2) != getIP())
                        continue;   //if they are different continue with the next interface

                    //otherwise get the MAC address for this interface and stop the loop
                    if (ioctl(_sockfd, SIOCGIFHWADDR, &ifr) == 0) {
                        success = true;
                        break;
                    }
                }
            }
        }
        else
            throw SocketException("Error in getting MAC address", SocketError::getMac);
    }

    if (!success)
        //if no MAC address was found signal error
        throw SocketException("Error in getting MAC address", SocketError::getMac);

    auto *hwaddr = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data); //get mac address from struct ifreq

    std::stringstream mac;  //compose mac address string
    mac <<
        std::hex << (int) hwaddr[0] << ":" <<
        std::hex << (int) hwaddr[1] << ":" <<
        std::hex << (int) hwaddr[2] << ":" <<
        std::hex << (int) hwaddr[3] << ":" <<
        std::hex << (int) hwaddr[4] << ":" <<
        std::hex << (int) hwaddr[5];

    return std::move(mac.str());   //return MAC address as string
}

/**
 * TCP_socket local IP address (address of its used interface) getter method
 *
 * @return (used) IP address of this machine's network card
 *
 * @throws SocketException:
 *  <b>getIP</b> if it could not get the IP address
 *
 * @author Michele Crepaldi s269551
 */
std::string TCP_Socket::getIP() const{
    //create a temporary socket to be used to get the IP address

    int sock = socket(PF_INET, SOCK_DGRAM, 0);  //temporary socket
    struct sockaddr_in loopback{}; //loopback sockaddr_in

    if (sock == -1)
        throw SocketException("Could not create socket", SocketError::getIp);

    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   //using loopback ip address
    loopback.sin_port = htons(9);                 //using debug port

    //connect to loopback
    if (::connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        throw SocketException("Could not connect", SocketError::getIp);
    }

    socklen_t addrlen = sizeof(loopback);
    //get sock name
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        throw SocketException("Could not getsockname", SocketError::getIp);
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    //convert the address to human readable form
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == nullptr)
        throw SocketException("Could not inet_ntop", SocketError::getIp);

    return std::string(buf);
}

/**
 * TCP_Socket close connection method
 *
 * @author Michele Crepaldi s269551
 */
void TCP_Socket::closeConnection() {
    //if the socket is not already closed
    if(_sockfd != 0) {
        char buffer[1024];

        //to ensure that the other peer receives all the previous messages before closing the connection
        //first send a FIN to the socket (then the other peer will close the connection) and then
        //receive all the next messages (ignoring them) until a FIN from the other peer is received;
        //then close the connection

        //send FIN
        if(shutdown(_sockfd, SHUT_WR) != -1) {

            //receive all messages until FIN
            while (true) {

                //number of bytes received
                int res = recv(_sockfd, buffer, 1024, MSG_DONTWAIT);

                //we got FIN or some connection error
                if (res <= 0)
                    break;
            }
        }

        //close socket
        close(_sockfd);
    }
}

/**
 * TCP_Socket destructor
 *
 * @author Michele Crepaldi s269551
 */
TCP_Socket::~TCP_Socket() {
    //if the socket is not already closed
    if(_sockfd != 0) {
        char buffer[1024];

        //to ensure that the other peer receives all the previous messages before closing the connection
        //first send a FIN to the socket (then the other peer will close the connection) and then
        //receive all the next messages (ignoring them) until a FIN from the other peer is received;
        //then close the connection

        //send FIN
        if(shutdown(_sockfd, SHUT_WR) != -1) {

            //receive all messages until FIN
            while (true) {

                //number of bytes received
                int res = recv(_sockfd, buffer, 1024, MSG_DONTWAIT);

                //we got FIN or some connection error
                if (res <= 0)
                    break;
            }
        }

        //close socket
        close(_sockfd);
    }
}

/**
 * TCP_ServerSocket constructor
 *
 * @param port port to use
 * @param n length of the listen queue
 *
 * @throws SocketException:
 *  <b>bind</b> if it could not bind the port
 *
 * @author Michele Crepaldi s269551
 */
TCP_ServerSocket::TCP_ServerSocket(unsigned int port, unsigned int n) {
    struct sockaddr_in sockaddrIn{};                //prepare struct sockaddr_in
    sockaddrIn.sin_port = htons(port);              //ensure network byte order
    sockaddrIn.sin_family = AF_INET;                //set address family
    sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY); //set address

    //bind the port and address to the socket
    if(::bind(_sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn)) != 0)
        throw SocketException("Cannot bind port", SocketError::bind);

    //open the socket (set it to listen)
    if(::listen(_sockfd, static_cast<int>(n)) != 0)
        throw SocketException("Cannot bind port", SocketError::bind);
}

/**
 * TCP_ServerSocket accept method
 *
 * @param addr address of the connected client
 * @param len length of the struct sockaddr_in
 * @return the newly created TCP_Socket
 *
 * @throws SocketException:
 *  <b>accept</b> if it could not accept a client socket
 *
 * @author Michele Crepaldi s269551
 */
SocketBridge* TCP_ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    //accept connection from TCP client and get its fd (changing addr and len)

    //accepted socket file descriptor
    int fd = ::accept(_sockfd, reinterpret_cast<struct sockaddr*>(addr), reinterpret_cast<socklen_t *>(len));
    if(fd < 0)
        throw SocketException("Cannot accept socket", SocketError::accept);

    return new TCP_Socket(fd);  //return a new TCP_Socket with the fd we just got
}

/**
 * TCP_ServerSocket destructor
 *
 * @author Michele Crepaldi s269551
 */
TCP_ServerSocket::~TCP_ServerSocket() {
    ::close(_sockfd);   //close the socket
}


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * (wolfSSL) TLS Socket implementation
 */

/**
 * TLS_Socket constructor
 *
 * @param sockfd socket file descriptor (of the underlying TCP socket) to assign to this TLS socket
 * @param ssl pointer to the WOLFSSL object to assign to this TLS socket
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket(int sockfd, WOLFSSL *ssl) :
    _sock(new TCP_Socket(sockfd)), _ssl(ssl) {   //assign to _sock a new TCP socket with the given fd and set _ssl
}

/**
 * TLS_Socket empty constructor
 *
 * @throws SocketException:
 *  <b>create</b> if the TlS_Socket could not be created OR there was an error in the initialization of the wolfSSL
 *  context or in loading the verifying CA
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket() {
    wolfSSL_Init();     //init wolfSSL library

    //crete wolfSSL context by using wolfTLS client method
    _ctx = tls_socket::UniquePtr<WOLFSSL_CTX>(wolfSSL_CTX_new(wolfTLS_client_method()));
    if(_ctx.get() == nullptr)
        throw SocketException("Error in initializing wolfSSL context", SocketError::create);

    //load verify CA from the filesystem
    if(wolfSSL_CTX_load_verify_locations(_ctx.get(), Socket::_ca_file_path.c_str(), nullptr) != SSL_SUCCESS) {
        std::stringstream errorMsg;
        errorMsg << "Error loading " << Socket::_ca_file_path << ", please check the file.";
        throw SocketException(errorMsg.str(), SocketError::create);
    }

    //set the TLS client to verify the server certificate
    wolfSSL_CTX_set_verify(_ctx.get(), SSL_VERIFY_PEER, nullptr);

    //set the certificate verify depth into the CA
    wolfSSL_CTX_set_verify_depth(_ctx.get(), 1);

    _sock = std::make_unique<TCP_Socket>();  //create TCP socket
}

/**
 * TLS_Socket move constructor
 *
 * @param other TLS_Socket to move
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket::TLS_Socket(TLS_Socket &&other) noexcept :
    _sock(other._sock.release()),   //assign to _sock the other socket's sock (freeing it)
    _ctx(other._ctx.release()),     //assign to _ctx the context of the other socket (freeing it)
    _ssl(other._ssl.release()) {    //assign to _ssl the other socket ssl (freeing it)
}

/**
 * TLS_Socket operator= (by movement) override
 *
 * @param other TLS_Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
TLS_Socket& TLS_Socket::operator=(TLS_Socket &&other) noexcept {
    if(_sock != nullptr) //if my sock has not been already closed (or never was opened)
        _sock.reset();   //close it

    //set the sock to be what the other's sock was
    _sock = std::unique_ptr<TCP_Socket>(other._sock.release());

    if(_ctx.get() != nullptr)   //if the context was set
        _ctx.reset();           //reset (free) it

    //set the context to be what the other's context was
    _ctx = tls_socket::UniquePtr<WOLFSSL_CTX>(other._ctx.release());

    if(_ssl.get() != nullptr)   //if the ssl object was set
        _ssl.reset();           //reset (free) it

    //set the ssl object to be what the other's ssl was
    _ssl = tls_socket::UniquePtr<WOLFSSL>(other._ssl.release());
    return *this;
}

/**
 * TLS_Socket connect method
 *
 * @param addr address of the TLS server to connect to
 * @param len length of the struct addr
 *
 * @throws SocketException:
 *  <b>connect</b> if it could not connect to remote TLS socket OR it could not create the wolfSSL ssl
 *
 * @author Michele Crepaldi s269551
 */
void TLS_Socket::connect(const std::string& addr, unsigned int port) {
    //connect underlying TCP socket
    _sock->connect(addr, port);

    //create a new wolfSSL object from the context
    _ssl = tls_socket::UniquePtr<WOLFSSL>(wolfSSL_new(_ctx.get()));
    if (_ssl.get() == nullptr)
        throw SocketException("Error in creating wolfSSL ssl", SocketError::connect);

    //associate the socket file descriptor to the wolfSSL object
    if(wolfSSL_set_fd(_ssl.get(), _sock->getSockfd()) != SSL_SUCCESS)
        throw SocketException("Cannot set fd to wolfSSL ssl", SocketError::connect);
}

/**
 * TLS_Socket read method
 *
 * @param buffer buffer where to put the read data
 * @param len length of the data to read
 * @return number of bytes read
 *
 * @throws SocketException:
 *  <b>read</b> if it could not receive data from the TLS_socket
 *
 * @author Michele Crepaldi s269551
 */
ssize_t TLS_Socket::read(char *buffer, size_t len) const {
    //receive len bytes from the socket and put it into buffer

    ssize_t res = wolfSSL_read(_ssl.get(), buffer, len);    //number of bytes read

    //if # of bytes read is < 0 throw exception
    if(res < 0) {
        char errorString[80];
        int err = wolfSSL_get_error(_ssl.get(), 0); //get error code
        wolfSSL_ERR_error_string(err, errorString); //get string from error code
        std::stringstream errMsg;
        errMsg << "Cannot read from socket. Error: " << errorString;
        throw SocketException(errMsg.str(), SocketError::read);
    }
    return res;
}

/**
 * TLS_Socket receive string method
 *
 * @return string read from TLS_Socket
 *
 * @throws SocketException:
 *  <b>read</b> if it could not read data from the TLS_Socket or data is less than expected
 *
 * @author Michele Crepaldi s269551
*/
std::string TLS_Socket::recvString() const {
    std::string stringBuffer;   //string buffer
    uint32_t dataLength;        //data length

    //receive first the message length

    ssize_t len = read(reinterpret_cast<char *>(&dataLength), sizeof(uint32_t));    //bytes read

    //if # of received bytes is less than expected throw exception
    if(len < sizeof(uint32_t))
        throw SocketException("Read from socket error, read bytes are less than expected", SocketError::read);

    dataLength = ntohl(dataLength); //ensure host system byte order

    std::vector<uint8_t> rcvBuf;        //receive buffer
    rcvBuf.resize(dataLength,0x00);  //initialize receive buffer with the necessary size

    len = read(reinterpret_cast<char *>(&(rcvBuf[0])), dataLength); //receive the data

    //if # of received bytes is less than expected throw exception
    if(len < dataLength)
        throw SocketException("Read from socket error, read bytes are less than expected", SocketError::read);

    //assign buffered data to a string
    stringBuffer.assign(reinterpret_cast<const char *>(&(rcvBuf[0])), rcvBuf.size());
    return stringBuffer;
}

/**
 * TLS_Socket write method
 *
 * @param buffer buffer where to get the data to write
 * @param len length of the data to write
 * @return number of bytes written
 *
 * @throws SocketException:
 *  <b>write</b> if it could not write data to the TLS_Socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t TLS_Socket::write(const char *buffer, size_t len) const {
    //send len bytes from buffer to the socket

    ssize_t res = wolfSSL_write(_ssl.get(), buffer, len);   //number of bytes written

    //if # of sent bytes is < 0 throw exception
    if(res < 0) {
        char errorString[80];
        int err = wolfSSL_get_error(_ssl.get(), 0);     //get error code
        wolfSSL_ERR_error_string(err, errorString);     //get error string from error code
        std::stringstream errMsg;
        errMsg << "Cannot write to socket. Error: " << errorString;
        throw SocketException(errMsg.str(), SocketError::write);
    }
    return res;
}

/**
 * TLS_Socket send string method
 *
 * @param stringBuffer string to write to TLS_Socket
 *
 * @throws SocketException:
 *  <b>write</b> if it could not write data to the TLS_Socket or if data written is less than expected
 *
 * @author Michele Crepaldi s269551
*/
ssize_t TLS_Socket::sendString(std::string &stringBuffer) const {
    //ensure network byte order when sending the data length

    uint32_t dataLength = htonl(stringBuffer.size());   //data to send size

    //send first the data length
    ssize_t len = write(reinterpret_cast<const char *>(&dataLength), sizeof(uint32_t));

    //if # of sent bytes is less than expected throw exception
    if(len < sizeof(uint32_t))
        throw SocketException("Write to socket error, sent bytes are less than expected", SocketError::write);

    //send the string data
    return write(stringBuffer.c_str(), stringBuffer.size());
}

/**
 * TLS_Socket socket file descriptor getter method
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int TLS_Socket::getSockfd() const {
    return _sock->getSockfd();
}

/**
 * TLS_socket MAC address getter method
 *
 * @return MAC address of this machine's network card
 *
 * @author Michele Crepaldi s269551
 */
std::string TLS_Socket::getMAC() const {
    return _sock->getMAC();
}

/**
 * TLS_socket local IP address (address of its used interface) getter method
 *
 * @return (used) IP address of this machine's network card
 *
 * @author Michele Crepaldi s269551
 */
std::string TLS_Socket::getIP() const{
    return _sock->getIP();
}

/**
 * TLS_Socket close connection method
 *
 * @author Michele Crepaldi s269551
 */
void TLS_Socket::closeConnection() {
    _sock.reset();   //close the socket
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
 * @param n length of the listen queue
 *
 * @throws SocketException:
 *  <b>create</b> if it could not create the wolfSSL context, or it could not load the certificate or private key files
 *  (or they are not valid)
 *
 * @author Michele Crepaldi s269551
 */
TLS_ServerSocket::TLS_ServerSocket(unsigned int port, unsigned int n) {
    //create a new wolfSSL context using the wolfTLS server method
    _ctx = tls_socket::UniquePtr<WOLFSSL_CTX>(wolfSSL_CTX_new(wolfTLS_server_method()));

    if (_ctx.get() == nullptr)
        throw SocketException("Cannot create server wolfSSL context", SocketError::create);

    wolfSSL_CTX_SetMinVersion(_ctx.get(), WOLFSSL_TLSV1_2);  //set minimum TLS version supported by the server

    //load the server certificate
    if (wolfSSL_CTX_use_certificate_file(_ctx.get(), ServerSocket::_certificate_path.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        std::stringstream errorMsg;
        errorMsg << "Error loading " << ServerSocket::_certificate_path << ", please check the file.";
        throw SocketException(errorMsg.str(), SocketError::create);
    }

    //load the server private key
    if (wolfSSL_CTX_use_PrivateKey_file(_ctx.get(), ServerSocket::_privatekey_path.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
        std::stringstream errorMsg;
        errorMsg << "Error loading " << ServerSocket::_privatekey_path << ", please check the file.";
        throw SocketException(errorMsg.str(), SocketError::create);   //throw exception if there are errors
    }

    //Check if the server certificate and private-key match
    if (!wolfSSL_CTX_check_private_key(_ctx.get()))
        //throw exception in case they don't match
        throw SocketException("Private key does not match the certificate public key", SocketError::create);

    //set the server to not ask the client for a certificate (no client TLS authentication)
    wolfSSL_CTX_set_verify(_ctx.get(), SSL_VERIFY_NONE, nullptr);

    _serverSock = std::make_unique<TCP_ServerSocket>(port, n);  //create new TCP server socket
}

/**
 * TLS_ServerSocket accept method
 *
 * @param addr address of the connected client
 * @param len length of the struct sockaddr_in
 * @return the newly created TLS_Socket
 *
 * @throws SocketException:
 *  <b>accept</b> if it could not create a new wolfSSL socket or associate the socket file descriptor to the wolfSSL ssl
 *
 * @author Michele Crepaldi s269551
 */
SocketBridge* TLS_ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    //accept underlying TCP socket

    auto s = _serverSock->accept(addr, len);    //client tcp socket

    //create new wolfSSL from context
    auto ssl = tls_socket::UniquePtr<WOLFSSL>(wolfSSL_new(_ctx.get()));
    if (ssl == nullptr)
        throw SocketException("Cannot create new wolfSSL socket", SocketError::accept);

    //associate the socket file descriptor to the newly created wolfSSL
    int err = wolfSSL_set_fd(ssl.get(), s->getSockfd());
    if(err != SSL_SUCCESS)
        throw SocketException("Cannot set fd to wolfSSL socket", SocketError::accept);

    //return a new TLS_Socket with the fd (and wolfSSL) we just got
    return new TLS_Socket(s->getSockfd(), ssl.release());
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
 * Socket class implementation
 */

//static variables declaration

std::string Socket::_ca_file_path;

/**
 * Socket static method to set the certificate path for the client socket (TLS)
 *
 * @param cacert path to the CA to check the server certificate with
 *
 * @author Michele Crepaldi s269551
 */
void Socket::specifyCertificates(const std::string &ca_file_path){
    _ca_file_path = ca_file_path;
}

/**
 * Socket constructor with SocketBridge pointer
 *
 * @param SocketBridge to create a new socket from
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(SocketBridge *sb) : _socket(std::unique_ptr<SocketBridge>(sb)) {
}

/**
 * Socket constructor with SocketType
 *
 * @param type type of the socket to be created:<ul>
 *  <li>socketType::TCP
 *  <li>socketType::TLS
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(SocketType type) {
    if(type == SocketType::TCP)
        _socket = std::make_unique<TCP_Socket>();
    else if(type == SocketType::TLS)
        _socket = std::make_unique<TLS_Socket>();
}

/**
 * Socket move constructor
 *
 * @param other Socket to move
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket(Socket &&other) noexcept :
    _socket(other._socket.release()){   //assign to _socket the other's socket (freeing it)
}

/**
 * Socket operator= (by movement) override
 *
 * @param other Socket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
Socket &Socket::operator=(Socket &&other) noexcept {
    if(_socket != nullptr)  //if _socket is not null
        _socket.reset();    //release it

    //assign to _socket the other's socket (freeing it)
    _socket = std::unique_ptr<SocketBridge>(other._socket.release());
    return *this;
}

/**
 * Socket connect method
 *
 * @param addr address of the server to connect to
 * @param len length of the struct addr
 *
 * @author Michele Crepaldi s269551
 */
void Socket::connect(const std::string &addr, unsigned int port) {
    _socket->connect(addr, port);
}

/**
 * Socket receive string method
 *
 * @return string read from Socket
 *
 * @author Michele Crepaldi s269551
*/
std::string Socket::recvString() const {
    return _socket->recvString();
}

/**
 * Socket send string method
 *
 * @param stringBuffer string to write to Socket
 *
 * @author Michele Crepaldi s269551
*/
ssize_t Socket::sendString(std::string &stringBuffer) const {
    return _socket->sendString(stringBuffer);
}

/**
 * Socket (tcp) file descriptor getter method
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int Socket::getSockfd() const {
    return _socket->getSockfd();
}

/**
 * Socket MAC address getter method
 *
 * @return MAC address of this machine's network card
 *
 * @author Michele Crepaldi s269551
 */
std::string Socket::getMAC() {
    if(_macAddress.empty())
        _macAddress = _socket->getMAC();

    return _macAddress;
}

/**
 * Socket local IP address (address of its used interface) getter method
 *
 * @return (used) IP address of this machine's network card
 *
 * @author Michele Crepaldi s269551
 */
std::string Socket::getIP(){
    if(_ipAddress.empty())
        _ipAddress = _socket->getIP();

    return _ipAddress;
}

/**
 * Socket close connection method
 *
 * @author Michele Crepaldi s269551
 */
void Socket::closeConnection() {
    _socket->closeConnection();
}

//static variables declaration

std::string ServerSocket::_certificate_path;
std::string ServerSocket::_privatekey_path;

/**
 * ServerSocket static method to set the certificate path for the client socket (TLS)
 *
 * @param certificate_path path to the server certificate
 * @param privatekey_path path to the server private key
 * @param cacert path to the CA to check the server certificate with
 *
 * @author Michele Crepaldi s269551
 */
void ServerSocket::specifyCertificates(const std::string &certificate_path, const std::string &privatekey_path,
                                       const std::string &ca_file_path){

    _certificate_path = certificate_path;
    _privatekey_path = privatekey_path;
    Socket::specifyCertificates(ca_file_path);
}

/**
 * ServerSocket constructor
 *
 * @param port port to use
 * @param n length of the listen queue
 *
 * @author Michele Crepaldi s269551
 */
ServerSocket::ServerSocket(unsigned int port, unsigned int n, SocketType type) : Socket(type) {
    if(type == SocketType::TCP)
        _serverSocket = std::make_unique<TCP_ServerSocket>(port, n);
    else if(type == SocketType::TLS)
        _serverSocket = std::make_unique<TLS_ServerSocket>(port, n);
}

/**
 * ServerSocket move constructor
 *
 * @param other ServerSocket to move
 *
 * @author Michele Crepaldi s269551
 */
ServerSocket::ServerSocket(ServerSocket &&other) noexcept :
    _serverSocket(other._serverSocket.release()) {  //assign to _serverSocket the other's serverSocket (freeing it)
}

/**
 * ServerSocket operator= (by movement) override
 *
 * @param other ServerSocket to move
 * @return this
 *
 * @author Michele Crepaldi s269551
 */
ServerSocket& ServerSocket::operator=(ServerSocket &&source) noexcept {
    if(_serverSocket != nullptr)    //if _serverSocket is not null
        _serverSocket.reset();      //release it

    //assign to _serverSocket the other's serverSocket (freeing it)
    _serverSocket = std::unique_ptr<ServerSocketBridge>(source._serverSocket.release());
    return *this;
}

/**
 * ServerSocket accept method
 *
 * @param addr address of the connected client
 * @param len length of the struct sockaddr_in
 * @return the newly created TLS_Socket
 *
 * @author Michele Crepaldi s269551
 */
Socket ServerSocket::accept(struct sockaddr_in *addr, unsigned long *len) {
    return Socket(_serverSocket->accept(addr, len));
}
