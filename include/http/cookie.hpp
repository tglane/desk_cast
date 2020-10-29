#ifndef HTTP_COOKIE_HPP
#define HTTP_COOKIE_HPP

#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <iostream>
#include <ctime>

namespace http
{

using std::string;

class cookie {

public:

    cookie() = default;
    cookie(const cookie&) = default;
    cookie& operator=(const cookie&) = default;
    cookie(cookie&&) = default;
    cookie& operator=(cookie&) = default;
    ~cookie() = default;

    cookie(string name, string value, bool httpOnly = false, bool secure = false, string comment = "",
        string domain = "", string max_age = "",string path = "/", int expires = 0);

    string build_header();

    void set_expiry_date(int days);

    void set_http_only(bool value) { m_http_only = value; }

    void set_secure(bool value) { m_secure = value; }

    string get_name() { return m_name; }

    string get_value() { return m_value; }

private:

    /**
     * Parameters of a http cookie
     * Not all headers are to set
     */
    string m_name;
    string m_value;
    bool m_http_only;
    bool m_secure;
    string m_comment;
    string m_domain;
    string m_max_age;
    string m_path;
    string m_expires;

};

} // namespace http

#endif
