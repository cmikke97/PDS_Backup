//
// Created by michele on 28/07/2020.
//

#include <vector>
#include <filesystem>
#include <fstream>
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
 * @throw SocketException in case the socket cannot be created
 *
 * @author Michele Crepaldi s269551
 */
Socket::Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        throw SocketException("Cannot create socket", socketError::create);
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
 * @throw SocketException in case it cannot receive data from socket
 *
 * @author Michele Crepaldi s269551
 */
ssize_t Socket::read(char *buffer, size_t len, int options) const {
    ssize_t res = recv(sockfd, buffer, len, options);
    if(res < 0)
        throw SocketException("Cannot read from socket", socketError::read);
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
 * @throw SocketException in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 *
 */
ssize_t Socket::write(const char *buffer, size_t len, int options) const {
    ssize_t res = send(sockfd, buffer, len, options);
    if(res < 0)
        throw SocketException("Cannot write to socket", socketError::write);
    return res;
}

/**
 * connect Socket to server
 *
 * @param addr of the server to connect to
 * @param len of the struct addr
 *
 * @throw SocketException in case it cannot connect to remote socket
 *
 * @author Michele Crepaldi s269551
 */
void Socket::connect(struct sockaddr_in *addr, unsigned int len) const {
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(addr), len) != 0)
        throw SocketException("Cannot connect to remote socket", socketError::connect);
}

/**
 * get the socket file descriptor
 *
 * @return sockfd
 *
 * @author Michele Crepaldi s269551
 */
int Socket::getSockfd() const {
    return sockfd;
}

/**
 * function to compose the address
 *
 * @param addr address of the server to connect to
 * @param port port of the server to connect to
 * @return sockaddr_in structure
 *
 * @author Michele Crepaldi s269551
*/
struct sockaddr_in Socket::composeAddress(const std::string& addr, int port) {
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, addr.c_str(), &(address.sin_addr));
    address.sin_port = htons(port);
    return address;
}

/**
 * funtion to write a string to a socket (useful with messages)
 *
 * @param stringBuffer string to write to socket
 * @param options
 *
 * @throw SocketException in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
*/
ssize_t Socket::sendString(std::string &stringBuffer, int options) const {
    uint32_t dataLength = htonl(stringBuffer.size()); // Ensure network byte order when sending the data length

    write(reinterpret_cast<const char *>(&dataLength), sizeof(uint32_t) , 0); // Send the data length
    return write(stringBuffer.c_str() ,stringBuffer.size() ,0); // Send the string data
}

/**
 * funtion to read a string from a socket (useful with messages)
 *
 * @param options
 * @return string read from socket
 *
 * @throw SocketException in case it cannot read data from socket
 *
 * @author Michele Crepaldi s269551
*/
std::string Socket::recvString(int options) const {
    std::string stringBuffer;
    uint32_t dataLength;
    read(reinterpret_cast<char *>(&dataLength), sizeof(uint32_t), options); // Receive the message length
    dataLength = ntohl(dataLength); // Ensure host system byte order

    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    rcvBuf.resize(dataLength,0x00); // with the necessary size

    read(reinterpret_cast<char *>(&(rcvBuf[0])), dataLength, options); // Receive the string data

    stringBuffer.assign(reinterpret_cast<const char *>(&(rcvBuf[0])), rcvBuf.size()); // assign buffered data to a string
    return stringBuffer;
}

/*
void Socket::recvFile(const std::filesystem::path& path, uintmax_t expectedFileSize, int options) {
    //initialize variables
    std::ofstream file;
    std::filesystem::directory_entry element(path);
    char buff[MAXBUFFSIZE];
    uintmax_t totalBytesReceived = 0;
    int justReceived;

    //open output file
    file.open(path, std::ios::out | std::ios::binary);

    if(file.is_open()){
        //receive file in MAXBUFFSIZE-wide blocks
        do{
            justReceived = this->read(buff, MAXBUFFSIZE, options);

            //write block to file and update total amount of bytes received
            totalBytesReceived += justReceived;
            file.write(buff, justReceived);
        }
        while(justReceived == MAXBUFFSIZE);
    }

    //close the output file
    file.close();

    //check total amout of bytes received against expected file size
    if(totalBytesReceived != expectedFileSize)
        throw std::runtime_error("received less bytes than expectedFileSize"); //if they are different then throw an exception
}*/

/**
 * function used to manually close the connection
 *
 * @author Michele Crepaldi s269551
 */
void Socket::closeConnection() const {
    if(sockfd != 0)
        close(sockfd);
}

/**
 * function used to get the machine MAC address for this socket
 *
 * @return mac address in string representation
 *
 * @throw SocketException in case of errors in MAC retrieval
 *
 * @author Michele Crepaldi s269551
 */
std::string Socket::getMAC() const {

    struct ifreq ifr{};
    struct ifconf ifc{};
    char buf[1024];
    bool success = false;

    //prepare ifc variable
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    //get the list of interface (transport layer) addresses associated to this socket (only for AF_INET (ipv4) addresses)
    if (ioctl(getSockfd(), SIOCGIFCONF, &ifc) == -1) {
        //error in getting the list
        throw SocketException("Error in getting MAC address", socketError::getMac);
    }

    //define iterator and end element
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* end = it + (ifc.ifc_len / sizeof(struct ifreq));

    //for each interface address in the previously computed list do this
    for (; it != end; ++it) {
        //copy the name of the interface as the ifreq struct name (subject of next instructions)
        strcpy(ifr.ifr_name, it->ifr_name);

        //get flags for the interface
        if (ioctl(getSockfd(), SIOCGIFFLAGS, &ifr) == 0) {

            //if the flag implies this is loopback interface then skip it
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) {

                //get the MAC address for this interface and stop the loop
                if (ioctl(getSockfd(), SIOCGIFHWADDR, &ifr) == 0) {
                    success = true;
                    break;
                }
            }
        }
        else {
            //error in getting of the flags
            throw SocketException("Error in getting MAC address", socketError::getMac);
        }
    }

    if (!success){
        //if no MAC address was found signal error
        throw SocketException("Error in getting MAC address", socketError::getMac);
    }

    //get mac address from struct ifreq
    auto *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data;

    //compose mac address string
    std::stringstream mac;
    mac <<
        std::setw(2) <<
        std::hex <<
        hwaddr[0] << ":" <<
        hwaddr[1] << ":" <<
        hwaddr[2] << ":" <<
        hwaddr[3] << ":" <<
        hwaddr[4] << ":" <<
        hwaddr[5];

    //return MAC address as string
    return mac.str();
}

/**
 * ServerSocket constructor
 *
 * @param port port to use
 *
 * @throw SocketException in case it cannot bind the port and create the socket
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
        throw SocketException("Cannot bind port", socketError::bind);
    if(::listen(sockfd, 8) != 0)
        throw SocketException("Cannot bind port", socketError::bind);

    //extract ip address in readable form
    char address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sockaddrIn.sin_addr, address, sizeof(address));
    std::cout << "Server opened: available at [" << address << ":" << port << "]" << std::endl;
}

/**
 * accept incoming connections from clients
 *
 * @param addr of the connected client
 * @param len of the struct addr
 * @return the newly created file descriptor (fd)
 *
 * @throw SocketException in case it cannot accept a socket
 *
 * @author Michele Crepaldi s269551
 */
Socket ServerSocket::accept(struct sockaddr_in *addr, unsigned int *len) {
    int fd = ::accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), reinterpret_cast<socklen_t *>(len));
    if(fd < 0)
        throw SocketException("Cannot accept socket", socketError::accept);
    return Socket(fd);
}
