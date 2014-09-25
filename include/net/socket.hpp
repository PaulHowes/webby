/**
 * @file socket.hpp
 */
#pragma once

#include <sstream>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

/**
 * @namespace net
 */
namespace net {
  /**
   * @brief Reports errors generated by net::socket and its subclasses.
   */
  class socket_error : public std::runtime_error {
    public:
      /**
       * @brief Constructs the `net::socket_error` object.
       * @param[in] what_arg Explanatory string.
       */
      explicit socket_error(const std::string& what_arg) : runtime_error(what_arg) { }

      /**
       * @brief Constructs the `net::socket_error` object.
       * @param[in] what_arg Explanatory string.
       */
      explicit socket_error(const char* what_arg) : runtime_error(what_arg) { }
  };

  /**
   * @brief Encapsulates a file descriptor used for socket communications.
   */
  class socket {
    public:
      /**
       * @brief Creates a new net::socket object.
       */
      socket() {
      }

      /**
       * @brief Creates a new net::socket object and associates it with an existing file
       *        descriptor
       * @param[in] fd Existing file descriptor to manage.
       */
      socket(int fd) : _fd(std::make_shared<int>(fd)) {
      }

      /**
       * @brief Create a new net::socket object and copies the shared file descriptor.
       */
      socket(const socket& s) : _fd(s._fd) {
      }

      /**
       * @brief Disconnect the net::socket object from the foreign host and destroys it.
       */
      ~socket() {
        if(_fd) {
          if(::close(*_fd) < 0) {
            std::ostringstream msg;
            msg << "Error closing socket: " << errno;
            throw socket_error(msg.str());
          }
        }
      }

      /**
       * @brief Reads a block of data from the connected host.
       * @param[in] buffer Buffer that receives the data.
       * @param[in] length Length of the buffer,
       * @param[in] peek   @c false to perform a normal read, @c true to read the data from the
       *                   connected host without removing it from the socket's input queue.
       * @returns The number of bytes actually read from the connected host.
       */
      unsigned read_block(char* buffer, const size_t length, const bool peek = false) const {
        ssize_t read_length = ::recv(*_fd, buffer, length, peek ? MSG_PEEK : 0);
        if(read_length < 0) {
          std::ostringstream msg;
          msg << "Error reading from connected host: " << errno;
          throw socket_error(msg.str());
        }
        return unsigned(read_length);
      }

      /**
       * @brief Reads a line of text from the connected host.
       * @returns A line of text read from the connected host.
       */
      std::string read_line() const {
        static const size_t BUFFER_SIZE = 4096;
        static char buffer[BUFFER_SIZE];

        // Reads a chunk of data from the socket without removing the data from socket's internal
        // buffer.
        unsigned read_length = this->read_block(buffer, BUFFER_SIZE, true);

        if(read_length > 0) {
          // Look for the EOL marker (CRLF) in the buffer. If found, the CRLF characters are
          // changed to NULL characters to mark the end of the string.
          unsigned found_eol = 0;
          char* p = buffer;
          while((p - buffer) < read_length && found_eol < 2) {
            if(*p == '\r' || *p == '\n') {
              found_eol++;
            }
            p++;
          }

          if(found_eol < 2) {
            throw socket_error("Line not found");
          }

          read_length = this->read_block(buffer, size_t(p - buffer));
          return std::string(buffer, size_t(read_length - 2));
        }

        return std::string();
      }

      /**
       * @brief Writes a block of data to the connected host.
       * @param[in] buffer Buffer that contains the data.
       * @param[in] length Length of the buffer,
       * @returns The number of bytes actually sent to the connected host.
       */
      unsigned write_block(const void* buffer, const size_t length) const {
        ssize_t write_length = ::send(*_fd, buffer, length, 0);
        if(write_length < 0) {
          std::ostringstream msg;
          msg << "Error writing to connected host: " << errno;
          throw socket_error(msg.str());
        }
        return unsigned(write_length);
      }

    protected:
      /**
       * @brief The actual socket (file) descriptor managed by the net::socket object.
       *
       * Using a shared pointer for an integer feels like a code smell, but it's the only way to
       * protect the file descriptor from having multiple owners or being closed before the last
       * owner is deleted.
       */
      std::shared_ptr<int> _fd;
  };

  class socket_impl : public socket {
    public:
      /**
       * @brief Constructs a new webby::socket_impl object.
       */
      socket_impl() : socket(), _res0(nullptr) {
      }

      /**
       * @brief Destroys this webby::socket_impl object.
       */
      ~socket_impl() {
        if(_res0) {
          freeaddrinfo(_res0);
        }
        socket::~socket();
      }

      /**
       * @brief Creates an endpoint for communicating with @p name and @p port.
       * @param[in] name Hostname or IP address of the endpoint.
       * @param[in] port Port for the endpoint.
       *
       * Both servers and clients use this method to create an endpoint. The difference is that
       * clients provide information about a remote host and servers the local host.
       */
      void create(std::string name, unsigned short port) {
        // Cannot create a socket if one already exists.
        if(_fd) {
          throw socket_error("Socket already exists.");
        }

        // The addrinfo structure found in netdb.h is used to define the socket type. Here the
        // socket is configured as a streaming TCP/IP server.
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_flags    = AI_PASSIVE;   // Socket will be passed to bind().
        hints.ai_family   = PF_INET;      // Internet Protocol (IP) socket.
        hints.ai_socktype = SOCK_STREAM;  // Streaming socket.
        hints.ai_protocol = IPPROTO_TCP;  // TCP protocol

        // Retrieves the socket address for the host. Here "localhost" is used so that the server
        // is available only on the current system. A real server would be accessible to the
        // outside world so the hostname would be either one that can be resolved to an external
        // IP, or simply NULL to be accessible on all attached networks. The second parameter is
        // the service (port number) that the server will listen on.
        int err = 0;
        if((err = ::getaddrinfo(name.c_str(), std::to_string(port).c_str(), &hints, &_res0))) {
          std::ostringstream msg;
          msg << "Could not resolve address: (" << err << ") " << gai_strerror(err);
          throw net::socket_error(msg.str());
        }

        // Now that all of the information for the socket is available, create it.
        _fd = std::make_shared<int>(::socket(_res0->ai_family, _res0->ai_socktype,
              _res0->ai_protocol));
        if(_fd < 0) {
          std::ostringstream msg;
          msg << "Could not create socket: " << errno;
          throw net::socket_error(msg.str());
        }
      }
      
    protected:
      /**
       * @brief Address of the endpoint.
       */
      struct addrinfo* _res0;
  };

  /**
   * @brief Extends net::socket to handle incoming connections from a server.
   *
   * Note that this is derived from socket, not socket_impl as the underlying file descriptor was
   * created by the OS when server::accept() was called.
   */
  class worker : public socket {
    public:
      /**
       * @brief Creates a new net::worker from an existing socket file descriptor.
       * @param[in] s File descriptor for the worker socket.
       * @param[in] a Address information for the connected client.
       * @param[in] l Length of the address information structure.
       *
       * This method should be invoked only by net::server.
       */
      worker(int s, struct sockaddr_in a, socklen_t l) :
          socket(s), _client_address(a), _address_length(l) {
      }

      /**
       * @brief Retrieves the hostname for the connected client.
       *
       * @returns Hostname for the connected client.
       */
      std::string client_hostname() const {
        struct hostent* client_host = gethostbyaddr(&_client_address.sin_addr.s_addr,
            sizeof(_client_address.sin_addr.s_addr), AF_INET);
        if(!client_host) {
          std::ostringstream msg;
          msg << "Could not get client hostname: " << h_errno << " " << hstrerror(h_errno);
          throw socket_error(msg.str());
        }
        return client_host->h_name;
      }

      /**
       * @brief Retrieves the IP address for the connected client.
       * @returns IP address for the connected client.
       */
      std::string client_ip() const {
        char* client_ip = inet_ntoa(_client_address.sin_addr);
        if(!client_ip) {
          throw socket_error("Could not get client IP.");
        }
        return client_ip;
      }

    private:
      /**
       * @brief Receives information about the connected client when net::server::accept() is
       *        called.
       */
      struct sockaddr_in _client_address;

      /**
       * @brief The number of entries in the client_address structure.
       */
      socklen_t _address_length;
  };

  /**
   * @brief Extends net::socket_impl to create a client.
   */
  class client : public socket_impl {
  };

  /**
   * @brief Extends net::socket_impl to create a server.
   */
  class server : public socket_impl {
    public:
      /**
       * @brief Constructs a new net::server object.
       */
      server() : socket_impl() { 
      }

      /**
       * @brief Creates a local endpoint at @p name and @p port that will accept incoming
       *        connections from client hosts.
       * @param[in] name Local hostname that the server will listen for connections from.
       * @param[in] port Local port that the server will listen for connections from.
       */
      void create(std::string name, unsigned short port) {
        socket_impl::create(name, port);
        
        // Allows the server to re-bind to the socket if the server is terminated and restarted
        // quickly (within the TIME_WAIT interval) as it takes time for the OS to notice that
        // this has happened. If "address in use" errors are seen, not using SO_REUSEADDR is
        // usually the cause.
        int reuse = 1;
        if(::setsockopt(*_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
          std::ostringstream msg;
          msg << "Could not configure socket: " << errno;
          throw net::socket_error(msg.str());
        }

        // Connects (binds) this process to the socket created by socket_impl.
        if(::bind(*_fd, _res0->ai_addr, _res0->ai_addrlen) < 0) {
          std::ostringstream msg;
          msg << "Could not bind to socket: " << errno;
          throw net::socket_error(msg.str());
        }

        // Listens for incoming connections. The second paremeter (backlog) is set to 10,000.
        // This is the number of connections that the operating system can queue up while a
        // request is being serviced. If the queue is full, then clients will receive a
        // "connection refused" error. If requests are not handled quickly enough, then queued
        // requests may time out.
        if(::listen(*_fd, 10000) < 0) {
          std::ostringstream msg;
          msg << "Could not listen for incoming connections: " << errno;
          throw net::socket_error(msg.str());
        }
      }

      /**
       * @brief Waits for a client connection.
       * 
       * Note that this is a blocking call if the net::server is not marked as non-blocking, or
       * not used within a @c select or @c poll construct.
       */
      worker accept() {
        struct sockaddr_in addr;
        socklen_t len = sizeof(struct sockaddr_in);
        int s = ::accept(*_fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
        if(s < 0) {
          std::ostringstream msg;
          msg << "Could not accept incoming connection: " << errno;
          throw socket_error(msg.str());
        }
        return worker(s, addr, len);
      }
  };
}
