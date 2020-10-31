#include <fstream>
#include <sstream>

#include <http/response.hpp>

namespace http
{

std::mutex response::c_file_mutex;

static std::string get_http_phrase(int status_code)
{
    // TODO
    return "OK";
}

std::string response::to_string()
{
    std::string response;

    /* Set some response fields if missing */
    if(m_code == 0)
    {
        m_code = 200;
    }
    if(m_headers.find("Content-Type") == m_headers.end())
    {
        m_headers.insert(std::pair<string, string>("Content-Type", "text/html; charset=UTF-8"));
    }
    std::time_t now(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

    /* Begin with response line */
    if(m_phrase.empty())
        response.append("HTTP/1.1 " + std::to_string(m_code) + " " + get_http_phrase(m_code) + "\r\n" + "Date: " + std::ctime(&now));
    else
        response.append("HTTP/1.1 " + std::to_string(m_code) + " " + m_phrase + "\r\n" + "Date: " + std::ctime(&now));

    /* Append all cookies to response */
    for(auto& it : m_cookies) //TODO auto& or auto?
    {
        response.append(it.second.build_header() + "\r\n");
    }

    /* Append all headers to response */
    for(auto& it : m_headers) //TODO auto& or auto
    {
        response.append(it.first + ": " + it.second + "\r\n");
    }

    /* Append body to response line */
    response.append("\r\n");
    if(!m_body.empty())
    {
        response.append(m_body);
    }

    return response;
}

void response::set_body_from_file(const std::string &bodyFile)
{
    /* Open template file and read it into a string if found */
    c_file_mutex.lock();
    if(bodyFile.rfind("..") != std::string::npos)
    {
        c_file_mutex.unlock();
        throw std::invalid_argument("Path contains not allowed characters!\n");
    }
    //TODO use only the filename and search only in allowed folders for the filename
    std::ifstream ifs(bodyFile);
    if(!ifs.good())
    {
        c_file_mutex.unlock();
        throw std::invalid_argument("Requested file not found.\n");
    }
    c_file_mutex.unlock();
    std::stringstream sstr;
    sstr << ifs.rdbuf();
    this->set_body(sstr.str());
}

void response::set_body(const std::string& body)
{
    m_body = body;
    set_header("Content-Length", std::to_string(m_body.size()));
}

void response::set_body(std::string&& body)
{
    m_body = std::move(body);
    set_header("Content-Length", std::to_string(m_body.size()));
}

void response::send_redirect(const std::string& url)
{
    this->set_header("Location", url);
    this->set_code(302);
}

void response::set_header(const std::string& key, const std::string& value)
{
    m_headers[key] = value;
}

void response::set_header(const std::string& key, std::string&& value)
{
    m_headers[key] = std::move(value);
}

void response::add_cookie(cookie&& cookie)
{
    m_cookies[cookie.get_name()] = std::move(cookie);
}

} // namespace http
