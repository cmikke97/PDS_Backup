//
// Created by Michele Crepaldi s269551 on 28/07/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef SOCKET_H
#define SOCKET_H

#include <memory>
#include <string>
#include <stdexcept>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * SocketBridge Interfaces
 */

/**
 * SocketBridge interface (users of the Socket library will "see" this class and none of the implementations)
 *
 *  <p>
 *  It contains pure abstract methods needed by a socket to work
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class SocketBridge {
public:
    //pure abstract methods
    virtual void connect(const std::string& addr, unsigned int port) = 0;
    [[nodiscard]] virtual std::string recvString() const = 0;
    virtual ssize_t sendString(std::string &stringBuffer) const = 0;
    [[nodiscard]] virtual int getSockfd() const = 0;
    [[nodiscard]] virtual std::string getMAC() const = 0;
    [[nodiscard]] virtual std::string getIP() const = 0;
    virtual void closeConnection() = 0;

    //virtual destructor
    virtual ~SocketBridge() = 0;
};

/**
 * ServerSocketBridge interface (users of the Socket library will "see" this class and none of the implementations)
 *
 *  <p>
 *  It contains pure abstract methods needed by a server socket to work.
 *  (it directly contains just the accept method, all other methods are publicly (and virtually)
 *  inherited from the SocketBridge class)
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class ServerSocketBridge : public virtual SocketBridge {
public:
    //pure abstract method
    virtual SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) = 0;
    ~ServerSocketBridge() override = 0;
};


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * TCP_Socket class implementation
 */

/**
 * Socket class (TCP)
 *
 *  <p>
 *  It implements all the abstract methods of the SocketBridge interface and some additional methods to make it work.
 *  It inherits publicly and virtually from SocketBridge interface
 *  </p>
 *  <p>
 *  <b>Not to be used directly
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class TCP_Socket : public virtual SocketBridge {
public:
    TCP_Socket(const TCP_Socket &) = delete;                //deleted copy constructor
    TCP_Socket& operator=(const TCP_Socket &) = delete;     //deleted copy assignment

    TCP_Socket();   //empty constructor
    TCP_Socket(TCP_Socket &&other) noexcept;            //move constructor
    TCP_Socket& operator=(TCP_Socket &&other) noexcept; //move assignment
    ~TCP_Socket() override; //destructor

    void connect(const std::string& addr, unsigned int port) override;  //connect method
    ssize_t read(char *buffer, size_t len, int options) const;          //read buffer method
    [[nodiscard]] std::string recvString() const override;              //receive string method
    ssize_t write(const char *buffer, size_t len, int options) const;   //write buffer method
    ssize_t sendString(std::string &stringBuffer) const override;       //send string method

    [[nodiscard]] int getSockfd() const override;       //get socket file descriptor method
    [[nodiscard]] std::string getMAC() const override;  //get MAC address of this machine's network card
    [[nodiscard]] std::string getIP() const override;   //get IP address of this machine's network card

    void closeConnection() override;    //close connection method

private:
    int _sockfd;     //soket file descriptor

    explicit TCP_Socket(int sockfd);    //explicit constructor (from socket file descriptor)

    friend class TCP_ServerSocket;
    friend class TLS_Socket;
};

/**
 * Server Socket class (TCP)
 *
 *  <p>
 *  It implements all the abstract methods of the ServerSocket interface and some additional methods to make it work.
 *  It inherits publicly from ServerSocketBridge interface and TCP_Socket class
 *  </p>
 *  <p>
 *  <b>Not to be used directly
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class TCP_ServerSocket: public ServerSocketBridge, public TCP_Socket{
public:
    TCP_ServerSocket(unsigned int port, unsigned int n);    //constructor with port and length of the listen queue
    ~TCP_ServerSocket() override;   //destructor

    SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) override;    //accept method
};


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * (wolfSSL) TLS_Socket class implementation
 */

/**
 * namespace containing the definition of the UniquePtr to use in the TLS classes
 *
 * @author Michele Crepaldi s269551
 */
namespace tls_socket {
    /**
     * custom deleter template for class T
     *
     * @tparam T class of the object to delete
     *
     * @author Michele Crepaldi s269551
     */
    template<class T>
    struct DeleterOf;

    /**
     * template specialization for the WOLFSSL class
     *
     * @author Michele Crepaldi s269551
     */
    template<>
    struct DeleterOf<WOLFSSL> {
        void operator()(WOLFSSL *p) const { wolfSSL_free(p); }
    };

    /**
     * template specialization for the wolfSSL_CTX class
     *
     * @author Michele Crepaldi s269551
     */
    template<>
    struct DeleterOf<WOLFSSL_CTX> {
        void operator()(WOLFSSL_CTX *p) const { wolfSSL_CTX_free(p); }
    };

    /*
     * definition of the UniquePtr construct, it is simply a unique_ptr object with the deleter for the
     * wolfSSLType template class redefined
     *
     * @author Michele Crepaldi s269551
     */
    template<class wolfSSLType>
    using UniquePtr = std::unique_ptr<wolfSSLType, DeleterOf<wolfSSLType>>;
}

/**
 * Secure Socket class (TLS)
 *
 *  <p>
 *  It implements all the abstract methods of the Socket interface and some additional methods to make it work.
 *  It inherits publicly and virtually from SocketBridge interface
 *  </p>
 *  <p>
 *  <b>Not to be used directly
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class TLS_Socket : public virtual SocketBridge {
public:
    TLS_Socket(const TLS_Socket &) = delete;                //delete copy constructor
    TLS_Socket& operator=(const TLS_Socket &) = delete;     //delete copy assignment

    TLS_Socket();   //empty constructor
    TLS_Socket(TLS_Socket &&other) noexcept;                //move constructor
    TLS_Socket& operator=(TLS_Socket &&other) noexcept;     //move assignment
    ~TLS_Socket() override; //destructor

    void connect(const std::string& addr, unsigned int port) override;  //connect method
    ssize_t read(char *buffer, size_t len) const;                       //read buffer method
    [[nodiscard]] std::string recvString() const override;              //receive string method
    ssize_t write(const char *buffer, size_t len) const;                //write buffer method
    ssize_t sendString(std::string &stringBuffer) const override;       //send string method

    [[nodiscard]] int getSockfd() const override;       //get socket file descriptor method
    [[nodiscard]] std::string getMAC() const override;  //get MAC address of this machine's network card
    [[nodiscard]] std::string getIP() const override;   //get IP address of this machine's network card

    void closeConnection() override;    //close connection method

private:
    std::unique_ptr<TCP_Socket> _sock;          //unique pointer to the underlying TCP socket
    tls_socket::UniquePtr<WOLFSSL_CTX> _ctx;    //Unique pointer to WOLFSSL_CTX
    tls_socket::UniquePtr<WOLFSSL> _ssl;        //Unique pointer to WOLFSSL

    TLS_Socket(int sockfd, WOLFSSL *ssl);   //constructor (from socket file descriptor and WOLFSSL object)

    friend class TLS_ServerSocket;
};

/**
 * Secure Server Socket class (TLS)
 *
 *  <p>
 *  It implements all the abstract methods of the ServerSocket interface and some additional methods to make it work.
 *  It inherits publicly from ServerSocketBridge interface and TLS_Socket class
 *  </p>
 *  <p>
 *  <b>Not to be used directly
 *  </p>
 *
 * @author Michele Crepaldi s269551
 */
class TLS_ServerSocket: public ServerSocketBridge, public TLS_Socket{
public:
    explicit TLS_ServerSocket(unsigned int port, unsigned int n);   //constructor with port and length of the listen queue
    ~TLS_ServerSocket() override;   //destructor

    SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) override;    //accept method

private:
    std::unique_ptr<TCP_ServerSocket> _serverSock;   //unique pointer to the underlying TCP server socket
};


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Socket class
 */

/**
 * SocketType class: it describes (enumerically) all the possible socket types (TCP or TLS)
 *
 * @author Michele Crepaldi s269551
 */
enum class SocketType{
    //TCP socket type
    TCP,

    //TLS socket type
    TLS};

/**
 * Socket class
 *
 *  <p>
 *  Class which implements the 2 possible sockets abstracting from implementation (bridge pattern):
 *  </p>
 *  <ul>
 *       <li>socketType::TCP for an unsecure TCP socket (make sure the server is using the same socket type!)
 *       <li>socketType::TLS for a secure TLS socket (make sure the server is using the same socket type!)
 *  </ul>
 *  <b>
 *  This class and the ServerSocket class are the only 2 to be used directly of this library.
 *  </b>
 *
 * @author Michele Crepaldi s269551
 */
class Socket {
public:
    static void specifyCertificates(const std::string &cacert);

    Socket(const Socket &) = delete;                //delete copy constructor
    Socket& operator=(const Socket &) = delete;     //delete copy assignment
    Socket() = default; //default constructor

    explicit Socket(SocketType type);   //explicit constructor with type

    Socket(Socket &&other) noexcept;                //move constructor
    Socket& operator=(Socket &&other) noexcept;     //move assignment

    void connect(const std::string& addr, unsigned int port);   //connect method
    [[nodiscard]] std::string recvString() const;               //receive string method
    ssize_t sendString(std::string &stringBuffer) const;        //send string method

    [[nodiscard]] int getSockfd() const;    //get socket file descriptor method
    [[nodiscard]] std::string getMAC();     //get MAC address of this machine's network card
    [[nodiscard]] std::string getIP();      //get IP address of this machine's network card

    void closeConnection(); //close connection method

private:
    std::unique_ptr<SocketBridge> _socket;  //unique pointer to SocketBridge object

    std::string _macAddress;    //mac address of this machine's network card
    std::string _ipAddress;     //ip address of this machine's network card

    explicit Socket(SocketBridge *sb);  //explicit constructor with pointer to SocketBridge object

    static std::string _ca_file_path;    //path to the certification authority file

    friend class ServerSocket;
    friend class TLS_Socket;
};

/**
 * Server Socket class
 *
 *  <p>
 *  Class which implements the 2 possible server sockets abstracting from implementation (bridge pattern):
 *  </p>
 *  <ul>
 *       <li>socketType::TCP for an unsecure TCP server socket (make sure the client is using the same socket type!)
 *       <li>socketType::TLS for a secure TLS server socket (make sure the client is using the same socket type!)
 *  </ul>
 *  <b>
 *  This class and the Socket class are the only 2 to be used directly of this library.
 *  </b>
 *
 * @author Michele Crepaldi s269551
 */
class ServerSocket : public Socket {
public:
    //specify certificates methods
    static void specifyCertificates(const std::string &cert, const std::string &prikey, const std::string &cacert);

    //constructor with port, length of the listen queue and type
    ServerSocket(unsigned int port, unsigned int n, SocketType type);

    ServerSocket(const ServerSocket &other) = delete;               //delete copy constructor
    ServerSocket& operator=(const ServerSocket& source) = delete;   //delete copy assignment

    ServerSocket(ServerSocket &&other) noexcept;                //move constructor
    ServerSocket& operator=(ServerSocket &&source) noexcept;    //move assignment

    Socket accept(struct sockaddr_in* addr, unsigned long* len);    //accept method

private:
    std::unique_ptr<ServerSocketBridge> _serverSocket;   //unique pointer to ServerSocketBridge object

    static std::string _certificate_path;    //path to the server certificate file
    static std::string _privatekey_path;     //path to the server private key file

    friend class TLS_ServerSocket;
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * SocketException
 */

/**
 * SokcetError class: it describes (enumerically) all the possible socket errors
 *
 * @author Michele Crepaldi s269551
 */
enum class SocketError{
    //could not create socket
    create,

    //could not bind socket
    bind,

    //could not accept client
    accept,

    //could not read from socket
    read,

    //could not write to socket
    write,

    //could not connect to server
    connect,

    //could not get mac address
    getMac,

    //could not get ip address
    getIp,

    //socket is closed
    closed
};

/**
 * SocketException exception class that may be returned by the Socket/ServerSocket class
 *  (derives from runtime_error)
 *
 * @author Michele Crepaldi s269551
 */
class SocketException : public std::runtime_error {
public:
    /**
     * socket exception constructor
     *
     * @param msg the error message
     *
     * @author Michele Crepaldi s269551
     */
    SocketException(const std::string& msg, SocketError code):
            std::runtime_error(msg), _code(code){
    }

    /**
     * socket exception destructor
     *
     * @author Michele Crepaldi s269551
     */
    ~SocketException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return socket error code
     *
     * @author Michele Crepaldi s269551
     */
    SocketError getCode() const noexcept{
        return _code;
    }

private:
    SocketError _code;   //code describing the error
};

#endif //SOCKET_H
