//
// Created by timog on 22.12.18.
//

#ifndef SOCKETWRAPPER_BASESOCKET_HPP
#define SOCKETWRAPPER_BASESOCKET_HPP

#include <memory>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <future>
#include <mutex>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h> //for struct sockaddr_in
#include <netdb.h> //for struct addrinfo
#include <unistd.h> //for close(), ...
#include <arpa/inet.h> //for inet_addr()

#include "BaseException.hpp"

namespace socketwrapper {

/**
 * @brief Simple socket wrapper base class that wraps the c socket functions into a c++ socket class
 */
class BaseSocket
{

public:

    enum class socket_state { SHUT, CLOSED, CREATED, BOUND };

    BaseSocket() = delete;

    virtual ~BaseSocket();

    //Block copying
    BaseSocket(const BaseSocket& other) = delete;
    BaseSocket& operator=(const BaseSocket& other) = delete;

    /**
     * Binds the internal Socket to your local adress and the given port
     * @param port to bind the socket on this port of the host machine
     * @throws SocketBoundException SocketBindException
     */
    void bind(const in_addr_t& address, int port);

    void bind(std::string_view address, int port);

    /**
     * @brief Closes the internal socket filedescriptor m_sockfd
     * @throws SocketCloseException
     */
    virtual void close();

    /**
     * @brief Returns the underlying socket descriptor to use it without the wrapping class
     * @return int
     */
    int get_socket_descriptor() const { return m_sockfd; }

    /**
     * @brief Returns the number of bytes available to read
     * @return int
     * @throws ReadBytesAvailableException
     */
    size_t bytes_available() const;

protected:

    BaseSocket(int family, int sock_type);

    BaseSocket(int socket_fd, int family, int sock_type, sockaddr_in own_addr, socket_state state);

    BaseSocket(BaseSocket&& other);

    BaseSocket& operator=(BaseSocket&& other);
    
    /**
     * Sets the internal socket file descriptor
     * @param int family
     * @param int sock_type
     * @return bool
     * @throws SocketCreationException SetSockOptException
     */
    bool create_new_file_descriptor();

    int resolve_hostname(const char* host_name, sockaddr_in* addr_out) const;

    mutable std::mutex m_mutex;

    int m_sockfd;

    int m_family;
    int m_socktype;

    sockaddr_in m_sockaddr_in;

    socket_state m_socket_state;
};

}

#endif //SOCKETWRAPPER_BASESOCKET_HPP
