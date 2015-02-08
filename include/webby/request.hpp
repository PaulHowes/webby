/**
 * @file request.hpp
 */

#pragma once

#include <map>
#include <webby/method.hpp>
#include <webby/utility.hpp>

/**
 * @namespace webby
 */
namespace webby {
  // Forward reference.
  class server;

  /**
   * @brief Representation of an HTTP request.
   */
  class request {
    public:
      /**
       * @brief Reports errors generated by webby::request and its subclasses.
       */
      class error : public std::runtime_error {
        public:
          /**
           * @brief Constructs the `request::error` object.
           * @param[in] what_arg Explanatory string.
           */
          explicit error(const std::string& what_arg) : runtime_error(what_arg) { }

          /**
           * @brief Constructs the `request::error` object.
           * @param[in] what_arg Explanatory string.
           */
          explicit error(const char* what_arg) : runtime_error(what_arg) { }
      };

      /**
       * @brief Gets a header value.
       * @param[in] name Name of the header.
       * @returns The value of the header.
       */
      const std::string& header(const std::string& name) {
        _config.error_log() << qlog::debug << "request::header()" << std::endl;
        return _header.at(name);
      }

      /**
       * @brief Gets a value that indicates whether a header is defined.
       * @param[in] name Name of the header to check.
       * @returns `true` if the header exists; otherwise `false`.
       */
      bool has_header(const std::string& name) {
        _config.error_log() << qlog::debug << "request::has_header()" << std::endl;
        return _header.count(name) == 1;
      }

      /**
       * @brief Gets the request method, e.g. @c GET/POST/HEAD etc.
       */
      webby::method method() const {
        _config.error_log() << qlog::debug << "request::method()" << std::endl;
        return _method;
      }

      /**
       * @brief Gets the request path.
       *
       * This is in the form @c /path/of/request
       */
      const std::string& path() const {
        _config.error_log() << qlog::debug << "request::path()" << std::endl;
        return _path;
      }

      /**
       * @brief Reads a block of data from the body of the request.
       * @param[in] buffer Buffer that receives the data.
       * @param[in] length Length of the buffer,
       * @param[in] peek   @c false to perform a normal read, @c true to read the data from the
       *                   request without removing it from the input queue.
       * @returns The number of bytes actually read from the request.
       */
      unsigned read_block(char* buffer, const size_t length, const bool peek = false) const {
        _config.error_log() << qlog::debug << "request::read_block()" << std::endl;
        return _worker.read(buffer, length, peek);
      }

      const std::string& route() const {
        _config.error_log() << qlog::debug << "request::route()" << std::endl;
        return _route;
      }

      /**
       * @brief Sets the route that caused this request to be invoked.
       * @param[in] route Route that caused this request to be invoked.
       * @returns Reference to this webby::response object for chaining.
       */
      request& set_route(const std::string& route) {
        _route = route;
        return *this;
      }

    protected:

      /**
       * @brief Constructs a new webby::request object from a @p worker socket.
       * @param[in] worker Worker socket used to communicate with the connected host.
       */
      request(const webby::config& config, const net::worker& worker) :
            _config(config), _worker(worker) {
        _config.error_log() << qlog::debug << "request::request()" << std::endl;
        process_request_line();
        process_header_lines();
      }

      /**
       * @brief Extracts the method and path from the first line of the request.
       * @throws webby::request::error if the request line was not valid.
       *
       * The first line of an HTTP request contains the method, path, and protocol in the following
       * format: "method [host[:port]]path HTTP/1.[0|1]"
       */
      void process_request_line() {
        _config.error_log() << qlog::debug << "request::process_request_line()" << std::endl;
        std::string request_line = _worker.read_line();
        std::string::const_iterator first = request_line.cbegin();
        std::string::const_iterator last = request_line.cbegin();

        // Finds the space that separates the method from the path.
        while(last != request_line.cend() && *last != ' ') {
          ++last;
        }

        // Throws an exception if the method could not be found.
        if(last == request_line.cend()) {
          std::ostringstream msg;
          msg << "Invalid request line: " << request_line;
          throw request::error(msg.str());
        }

        // Stores the method.
        std::string m = lowercase(std::string(first, last));
        first = last;

        if(m == "connect") {
          _method = method::CONNECT;
        }
        else if(m == "delete") {
          _method = method::DELETE;
        }
        else if(m == "get") {
          _method = method::GET;
        }
        else if(m == "head") {
          _method = method::HEAD;
        }
        else if(m == "options") {
          _method = method::OPTIONS;
        }
        else if(m == "post") {
          _method = method::POST;
        }
        else if(m == "put") {
          _method = method::PUT;
        }
        else if(m == "trace") {
          _method = method::TRACE;
        }

        _config.error_log() << qlog::debug << "  Request Method: " << m << std::endl;

        // Finds the first "/" character in the path.
        while(first != request_line.cend() && *first != '/') {
          ++first;
        }

        // Throws an exception if the beginning of the path could not be found.
        if(first == request_line.cend()) {
          std::ostringstream msg;
          msg << "Invalid request line: " << request_line;
          throw request::error(msg.str());
        }

        last = first;

        // Finds the space that separates the path from the protocol.
        while(last != request_line.cend() && *last != ' ') {
          ++last;
        }

        // Throws an exception if the path could not be found.
        if(last == request_line.cend()) {
          std::ostringstream msg;
          msg << "Invalid request line: " << request_line;
          throw request::error(msg.str());
        }

        // Saves the path.
        _path = std::string(first, last);
        _config.error_log() << qlog::debug << "  Request Path: " << _path << std::endl;
      }

      /**
       * @brief Extracts headers from the request
       *
       * Each header has the format, "header-name: value". The name is case insensitive. The line
       * is terminated with a CRLF. It is possible to split a long value over multiple lines by
       * terminating each line with a comma. For example, the following two headers are identical:
       *
       *     header1: value 1, value 2
       *     HEADER1: value 1,
       *              value 2
       *
       * The headers appear immediately after the HTTP request line, and are separated from the
       * request body by a blank line that is terminated with a CRLF.
       *
       * NOTE: Invalid headers (those that could not be parsed properly) are ignored rather than
       * throwing exceptions as this does not affect the server itself, only request handlers.
       */
      void process_header_lines() {
        _config.error_log() << qlog::debug << "request::process_header_lines()" << std::endl;
        std::string header_line = _worker.read_line();
        std::string name;

        // The request headers are separated from the request body by a blank line.
        while(header_line.length() > 0) {
          std::string::const_iterator itr = header_line.cbegin();

          // Skips past whitespace at the beginning of the line.
          while(itr != header_line.cend() && *itr == ' ') {
            ++itr;
          }

          // Removes the whitespace.
          header_line = std::string(itr, header_line.cend());
          itr = header_line.cbegin();

          // If the value of the previous header ends in a comma, then this entire line is
          // appended to that value.
          if(name.length() > 0 && *(_header[name].crend()) == ',') {
            _header[name] = _header[name] + std::string(" ") + header_line;
          }

          else {
            // Search for the colon that separates the name from the value.
            while(itr != header_line.cend() && *itr != ':') {
              ++itr;
            }

            if(itr != header_line.cend()) {
              // Saves the name of the header.
              name = std::string(header_line.cbegin(), itr);

              // Finds the value.
              ++itr;
              while(itr != header_line.cend() && *itr == ' ') {
                ++itr;
              }

              // Saves the key and the value in the map
              _header[name] = std::string(itr, header_line.cend());
            }
          }

          header_line = _worker.read_line();
        }

        for(auto itr = _header.cbegin(); itr != _header.cend(); ++itr) {
          _config.error_log() << qlog::debug << "  " << (*itr).first << ": " << (*itr).second <<
              std::endl;
        }
      }

    // Fields.
    private:
      /**
       * @brief server configuration.
       */
      const webby::config& _config;

      /**
       * @brief Worker socket.
       */
      const net::worker& _worker;

      /**
       * @brief Request method.
       */
      webby::method _method;

      /**
       * @brief Path of the request.
       */
      std::string _path;

      /**
       * @brief Route that caused the request to be invoked.
       */
      std::string _route;

      /**
       * @brief Headers.
       */
      std::map<std::string, std::string, no_case_compare> _header;

    // Friends
    friend class webby::server;
  };
}
