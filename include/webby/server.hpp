/**
 * @file
 */
#pragma once

#include <net/socket.hpp>

#include <webby/config.hpp>
#include <webby/request.hpp>
#include <webby/response.hpp>
#include <webby/router.hpp>

namespace webby {
  /**
   * @brief Exception object used for fatal server errors.
   *
   * The `webby::server_error` class represents fatal errors encountered by `webby::server` that
   * cannot be recovered from.
   */
  class server_error : public std::runtime_error {
    public:
      /**
       * @brief Constructs the `webby::server_error` object.
       * @param[in] what_arg Explanatory string.
       */
      explicit server_error(const std::string& what_arg) : runtime_error(what_arg) { }

      /**
       * @brief Constructs the `webby::server_error` object.
       * @param[in] what_arg Explanatory string.
       */
      explicit server_error(const char* what_arg) : runtime_error(what_arg) { }
  };

  /**
   * @brief Server object that the client interacts with.
   */
  class server {
    public:
      /**
       * @brief Default constructor
       */
      server() : _config(std::move(webby::config())), _router(std::move(webby::router())) {
        _config.error_log() << qlog::debug << "server::server()" << std::endl;
        try {
          init();
        }
        catch(const webby::server_error& e) {
          _config.error_log() << qlog::error << e.what() << std::endl;
          throw;
        }
      }

      /**
       * @brief Constructor that accepts a server configuration.
       * @param[in] config Server configuration.
       */
      server(const webby::config& config, const webby::router& router)
            : _config(config), _router(router) {
        _config.error_log() << qlog::debug
                            << "server::server(const webby::config&)" << std::endl;
        try {
          init();
        }
        catch(const webby::server_error& e) {
          _config.error_log() << qlog::error << e.what() << std::endl;
          throw;
        }
      }

      /**
       * @brief Destructor
       */
      ~server() {
        _config.error_log() << qlog::debug << "server::~server" << std::endl;
      }

      /**
       * @brief Runs the server.
       */
      void run() {
        _config.error_log() << qlog::debug << "server::run()" << std::endl;

        // The base implementation of the server is the simplest possible: An infinite loop that
        // blocks on the server::accept() call until a client connects.
        while(1) {
          // Accept the incoming connection and create a worker socket for it.
          net::worker worker = _server.accept();

          // Some connection logging.
          _config.error_log() << qlog::debug << "Accepted connection" << std::endl;
          _config.error_log() << qlog::debug << "  Client Hostname: " << worker.client_hostname()
              << std::endl;
          _config.error_log() << qlog::debug << "  Client IP: " << worker.client_ip() << std::endl;

          // Decompose the HTTP request from the client.
          request req(_config, worker);

          // Create the default response for the handler to populate.
          response res(_config, worker);

          // Populates some default headers.
          if(req.has_header("Host")) {
            std::ostringstream location;
            location << "http://" << req.header("Host") << req.path();
            res.set_header("Location", location.str());
          }

          // Routes the request to a handler.
          _router.route(req, res);
        }
      }

    private:
      /** Server configuration. */
      const webby::config& _config;

      /**
       * @brief Request router.
       */
      const webby::router& _router;

      /**
       * @brief Initializes the server.
       */
      void init() {
        _config.error_log() << qlog::debug << "server::init()" << std::endl;
        _server.create(_config.address(), _config.port());
      }

      /**
       * @brief Server socket.
       */
      net::server _server;
  };
}
