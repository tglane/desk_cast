#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <socketwrapper/TCPSocket.hpp>
#include <string>
#include <map>
#include <memory>
#include <list>
#include <variant>
#include <exception>
#include <mutex>

#include <http/request.hpp>

namespace http
{

class response
{
public:

    response() = default;

    explicit response(const request& req) 
        : m_req {req}
    {}

    std::string to_string();

    void set_body_from_file(const std::string& bodyFile);

    void send_redirect(const std::string& url);

    void set_header(const std::string& key, const std::string& value);

    void set_header(const std::string& key, std::string&& value);

    void add_cookie(cookie&& cookie);

    void set_code(int code)
    {
        m_code = code;
    }

    void set_code(int code, std::string&& phrase) 
    {
        m_code = code; m_phrase = std::move(phrase);
    }

    void set_body(const std::string& body);

    void set_body(std::string&& body);

    int get_code() const
    {
        return m_code;
    }

private:

    request m_req;                     /// Request from the client to answer with this response

    int m_code = 0;
    std::string m_phrase;
    std::string m_body;

    std::map<std::string, std::string> m_headers;
    std::map<std::string, cookie> m_cookies;

    static std::mutex c_file_mutex;

};

} // namespace http

#endif
