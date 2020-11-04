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
#include <memory>
#include <cstdio>
#include <string>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Interfaces
 */

/**
 * Socket interface (users of this library will see this class and none of the implementations)
 *
 * <p>
 * It contains all methods needed by a socket to work, most of them are abstract;
 * just one function (which is static) is actually implemented (it is independent from all implementations)
 * and it is the getMAC method
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class SocketBridge {
public:
    //pure abstract methods
    virtual void connect(const std::string& addr, int port) = 0;
    [[nodiscard]] virtual std::string recvString() const = 0;
    virtual ssize_t sendString(std::string &stringBuffer) const = 0;
    [[nodiscard]] virtual int getSockfd() const = 0;
    [[nodiscard]] virtual std::string getMAC() const = 0;
    virtual void closeConnection() = 0;
};

/**
 * ServerSocket interface (users of this library will see this class and none of the implementations)
 *
 * <p>
 * It contains all methods needed by a socket to work, all of them are abstract;
 * (it directly contains just the accept method, all other methods are publicly (and virtually)
 * inherited from the Socket class
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class ServerSocketBridge : public virtual SocketBridge {
public:
    //pure abstract method
    virtual SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) = 0;
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * TCP Socket implmementation
 */

/**
 * Socket class (TCP)
 *
 * <p>
 * It implements all the abstract methods of the Socket interface and some additional methods to make it work.
 * </p>
 * <p>
 * <b>Not to be used directly
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class TCP_Socket : public virtual SocketBridge {
    int sockfd;

    explicit TCP_Socket(int sockfd);
    friend class TCP_ServerSocket;
    friend class TLS_Socket;

public:
    TCP_Socket(const TCP_Socket &) = delete;
    TCP_Socket& operator=(const TCP_Socket &) = delete;

    TCP_Socket();
    TCP_Socket(TCP_Socket &&other) noexcept;
    TCP_Socket& operator=(TCP_Socket &&other) noexcept;
    void connect(const std::string& addr, int port) override;
    ssize_t read(char *buffer, size_t len, int options) const;
    [[nodiscard]] std::string recvString() const override;
    ssize_t write(const char *buffer, size_t len, int options) const;
    ssize_t sendString(std::string &stringBuffer) const override;
    [[nodiscard]] int getSockfd() const override;
    [[nodiscard]] std::string getMAC() const override;
    void closeConnection() override;
    ~TCP_Socket();
};

/**
 * Server Socket class (TCP)
 *
 * <p>
 * It implements all the abstract methods of the ServerSocket interface and some additional methods to make it work.
 * </p>
 * <p>
 * <b>Not to be used directly
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class TCP_ServerSocket: public ServerSocketBridge, public TCP_Socket{
public:
    explicit TCP_ServerSocket(int port, int n);
    SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) override;
    ~TCP_ServerSocket();
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * WolfSSL TLS Socket implmementation
 */

/**
 * namespace containing the definition of the UniquePtr to use in the TLS classes
 *
 * @author Michele Crepaldi s269551
 */
namespace tls_socket {
    template<class T>
    struct DeleterOf;

    template<>
    struct DeleterOf<WOLFSSL> {
        void operator()(WOLFSSL *p) const { wolfSSL_free(p); }
    };

    template<>
    struct DeleterOf<WOLFSSL_CTX> {
        void operator()(WOLFSSL_CTX *p) const { wolfSSL_CTX_free(p); }
    };

    template<class wolfSSLType>
    using UniquePtr = std::unique_ptr<wolfSSLType, DeleterOf<wolfSSLType>>;
}

/**
 * Secure Socket class (TLS)
 *
 * <p>
 * It implements all the abstract methods of the Socket interface and some additional methods to make it work.
 * </p>
 * <p>
 * <b>Not to be used directly
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class TLS_Socket : public virtual SocketBridge {
    std::unique_ptr<TCP_Socket> sock;
    tls_socket::UniquePtr<WOLFSSL_CTX> ctx;
    tls_socket::UniquePtr<WOLFSSL> ssl;

    explicit TLS_Socket(int sockfd, WOLFSSL *ssl);
    friend class TLS_ServerSocket;

public:
    TLS_Socket(const TLS_Socket &) = delete;
    TLS_Socket& operator=(const TLS_Socket &) = delete;

    TLS_Socket();
    TLS_Socket(TLS_Socket &&other) noexcept;
    TLS_Socket& operator=(TLS_Socket &&other) noexcept;
    void connect(const std::string& addr, int port) override;
    ssize_t read(char *buffer, size_t len) const;
    [[nodiscard]] std::string recvString() const override;
    ssize_t write(const char *buffer, size_t len) const;
    ssize_t sendString(std::string &stringBuffer) const override;
    [[nodiscard]] int getSockfd() const override;
    [[nodiscard]] std::string getMAC() const override;
    void closeConnection() override;
    ~TLS_Socket();
};

/**
 * Secure Server Socket class (TLS)
 *
 * <p>
 * It implements all the abstract methods of the ServerSocket interface and some additional methods to make it work.
 * </p>
 * <p>
 * <b>Not to be used directly
 * </p>
 *
 * @author Michele Crepaldi s269551
 */
class TLS_ServerSocket: public ServerSocketBridge, public TLS_Socket{
    std::unique_ptr<TCP_ServerSocket> serverSock;
public:
    explicit TLS_ServerSocket(int port, int n);
    SocketBridge* accept(struct sockaddr_in* addr, unsigned long* len) override;
    ~TLS_ServerSocket();
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * socket class
 */

/**
 * sokcetType class: it describes (enumerically) all the possible socket types (TCP or TLS)
 *
 * @author Michele Crepaldi s269551
 */
enum class socketType{TCP, TLS};

/**
 * Socket class
 *
 * <p>
 * Class which implements the 2 possible sockets abstracting from implementation (bridge pattern):
 * </p>
 * <ul>
 *      <li>socketType::TCP for an unsecure TCP socket (make sure the server is using the same socket type!)
 *      <li>socketType::TLS for a secure TLS socket (make sure the server is using the same socket type!)
 * </ul>
 * <b>
 * This class and the ServerSocket class are the only 2 to be used directly of this library.
 * </b>
 *
 * @author Michele Crepaldi s269551
 */
class Socket {
    std::unique_ptr<SocketBridge> socket;
    explicit Socket(SocketBridge *sb);
    friend class ServerSocket;

public:
    static std::string ca_file_path;
    static void specifyCertificates(const std::string &cacert);

    Socket(const Socket &) = delete;
    Socket& operator=(const Socket &) = delete;
    Socket() = default;

    explicit Socket(socketType type);
    Socket(Socket &&other) noexcept;
    Socket& operator=(Socket &&other) noexcept;
    void connect(const std::string& addr, int port);
    [[nodiscard]] std::string recvString() const;
    ssize_t sendString(std::string &stringBuffer) const;
    [[nodiscard]] int getSockfd() const;
    [[nodiscard]] std::string getMAC();
    void closeConnection();
};

/**
 * Server Socket class
 *
 * <p>
 * Class which implements the 2 possible server sockets abstracting from implementation (bridge pattern):
 * </p>
 * <ul>
 *      <li>socketType::TCP for an unsecure TCP server socket (make sure the client is using the same socket type!)
 *      <li>socketType::TLS for a secure TLS server socket (make sure the client is using the same socket type!)
 * </ul>
 * <b>
 * This class and the Socket class are the only 2 to be used directly of this library.
 * </b>
 *
 * @author Michele Crepaldi s269551
 */
class ServerSocket : public Socket {
    std::unique_ptr<ServerSocketBridge> serverSocket;

public:
    static std::string certificate_path, privatekey_path;
    static void specifyCertificates(const std::string &cert, const std::string &prikey, const std::string &cacert);

    explicit ServerSocket(int port, int n, socketType type);
    Socket accept(struct sockaddr_in* addr, unsigned long* len);
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * exceptions
 */

/**
 * sokcetError class: it describes (enumerically) all the possible socket errors
 *
 * @author Michele Crepaldi s269551
 */
enum class socketError{create, bind, accept, read, write, connect, getMac};

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
