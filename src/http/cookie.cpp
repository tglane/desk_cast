#include <http/cookie.hpp>

namespace http
{

cookie::cookie(string name, string value, bool httpOnly, bool secure, string comment, string domain, string max_age,
               string path, int expires)
{
    m_name = std::move(name);
    m_value = std::move(value);
    m_http_only = httpOnly;
    m_secure = secure;
    m_comment = std::move(comment);
    m_domain = std::move(domain);
    m_max_age = std::move(max_age);
    m_path = std::move(path);
    if(expires > 0) { set_expiry_date(expires); }
}

string cookie::build_header()
{
    string header = "Set-Cookie: " + m_name + "=" + m_value;

    if(!m_comment.empty()) { header.append("; Comment=" + m_comment); }
    if(!m_domain.empty()) { header.append("; Domain=" + m_domain); }
    if(!m_max_age.empty()) { header.append("; Max-Age=" + m_max_age); }
    if(!m_path.empty()) { header.append("; Path=" + m_path); }
    if(m_http_only) { header.append("; HttpOnly"); }
    if(m_secure) { header.append("; secure"); }
    if(!m_expires.empty()) { header.append("; Expires=" + m_expires); }

    return header;
}

void cookie::set_expiry_date(int days)
{
    std::chrono::time_point<std::chrono::system_clock> e(std::chrono::system_clock::now() + std::chrono::hours(days * 24));
    std::time_t t(std::chrono::system_clock::to_time_t(e));
    m_expires = std::ctime(&t);
}

} // namespace http
