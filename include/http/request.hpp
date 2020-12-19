#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <string_view>
#include <sstream>
#include <istream>
#include <iterator>
#include <map>
#include <vector>
#include <memory>

#include "http/cookie.hpp"

namespace http {

class request {

public:

    request() = default;
    request(const request& other) = default;
    request(request&& other) noexcept;
    request& operator=(const request& other) = default;
    request& operator=(request&& other);

    explicit request(const char* request_string);

    void parse(const char *request);

    std::string to_string() const;

    bool check_header(const std::string& key) const;

    std::map<std::string_view, std::string_view> get_headers() const { return m_headers; }

    std::string get_header(const std::string& key) const;

    std::string get_method() const { return std::string(m_method); }

    std::string get_resource() const { return std::string(m_resource); }

    std::string get_protocol() const { return std::string(m_protocol); }

    std::string get_path() const { return std::string(m_path); }

    std::map<std::string_view, std::string_view> get_params() const { return m_query_params; }

    std::string get_param(const std::string& key) const;

    std::map<std::string, cookie> get_cookies() const { return m_cookies; }

    cookie get_cookie(const std::string &cookie_name) const;

    std::map<std::string_view, std::string_view> get_post_params() const { return m_body_params; }

    std::string get_post_param(const std::string &key) const;

private:

    void parse_requestline(std::string_view requestline);

    static void parse_params(std::string_view param_string, std::map<std::string_view, std::string_view>& param_container);

    void parse_cookies(const std::string &cookies);

    std::string m_request;    /// unparsed request

    std::string_view m_method;     /// http method used by this request (e.g. post, get, ...)
    std::string_view m_protocol;   /// protocol of this request - should be HTTP/*.*
    std::string_view m_resource;   /// resource addressed by this request
    std::string_view m_path;       /// path of the resource addressed by this request
    std::string_view m_fragment;   /// TODO parse fragment?!

    std::map<std::string_view, std::string_view> m_query_params; /// contains names and values of the query string
    std::map<std::string_view, std::string_view> m_headers; /// contains names and values of the http request headers
    std::map<std::string, cookie> m_cookies; /// contains names and values of the cookies
    std::map<std::string_view, std::string_view> m_body_params; /// Contains post params from the requests body

};

} // namespace http

#endif
