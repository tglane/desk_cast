#include "http/request.hpp"

#include <charconv>

namespace http
{

request::request(const char* unparsed_request)
{
    try {
        parse(unparsed_request);
    } catch(const std::invalid_argument& ia) { throw ia; }
}

request::request(request&& other) noexcept
{
    *this = std::move(other);
}

request& request::operator=(request&& other)
{
    this->m_request = std::move(other.m_request);
    this->m_method = std::move(other.m_method);
    this->m_protocol = std::move(other.m_protocol);
    this->m_resource = std::move(other.m_resource);
    this->m_path = std::move(other.m_path);
    this->m_fragment = std::move(other.m_fragment);
    this->m_query_params = std::move(other.m_query_params);
    this->m_headers = std::move(other.m_headers);
    this->m_cookies = std::move(other.m_cookies);
    this->m_body_params = std::move(m_body_params);

    other.m_method = std::string_view {other.m_request.data()};
    other.m_protocol = std::string_view {other.m_request.data()};
    other.m_resource = std::string_view {other.m_request.data()};
    other.m_path = std::string_view {other.m_request.data()};
    other.m_fragment = std::string_view {other.m_request.data()};
    other.m_query_params.clear();
    other.m_headers.clear();
    other.m_cookies.clear();
    other.m_body_params.clear();

    return *this;
}

void request::parse(const char* request)
{
    size_t pos_q;
    m_request = std::string {request};

    /* extract the request line */
    // std::istringstream request_stringstream(m_request);
    // std::string request_line;
    // getline(request_stringstream, request_line);
    // this->parse_requestline(request_line);

    // std::string_view full_request_view(m_request);
    pos_q = m_request.find("\r\n");
    if(pos_q == std::string::npos)
        throw std::invalid_argument("invalid_request");
    this->parse_requestline(std::string_view {m_request.data(), pos_q + 1});

    /* Parse resource to path and params */
    if((pos_q = m_resource.find('?')) == std::string::npos)
    {
        m_path = m_resource;
    }
    else
    {
        m_path = std::string_view {m_resource.data(), pos_q};
        request::parse_params(std::string_view (m_resource.data() + pos_q + 1, m_protocol.data() - (m_resource.data() + pos_q + 1) - 1), m_query_params);
    }

    /* Read and parse request headers */
    size_t start_pos = m_request.find("\r\n") + 2, mid_pos, end_pos;
    std::string_view headerline {m_request.data() + start_pos};
    while(headerline != "\r\n" || headerline != "\r" || headerline != "\n")
    {
        mid_pos = headerline.find(':');
        end_pos = headerline.find("\r\n");
        if(end_pos == 0)
            break;

        if(start_pos == std::string_view::npos || mid_pos == std::string_view::npos || end_pos == std::string_view::npos)
            throw std::invalid_argument {"invalid_request"};

        m_headers.insert({ std::string_view {headerline.data(), mid_pos}, std::string_view {headerline.data() + mid_pos + 2, end_pos - (mid_pos + 2)} });
 
        start_pos = end_pos;
        headerline = std::string_view {headerline.data() + end_pos + 2};
    }

    /* Read cookies and put it in m_cookies */
    auto it = m_headers.find("Cookie");
    if(it != m_headers.end())
    {
        this->parse_cookies(it->second.data());
    }

    /* Parse POST parameters */
    if(m_method == "POST" || m_method == "post")
    {
        int length;
        std::string_view hdr = get_header("Content-Length");
        auto res = std::from_chars(hdr.data(), hdr.data() + hdr.size(), length);
        if(res.ec != std::errc::invalid_argument && length > 0)
        {
            std::string_view body {headerline.data() + end_pos + 2};
            request::parse_params(body, m_body_params);
        }
    }
}

std::string request::to_string() const
{
    if(!m_request.empty())
        return m_request;

    // TODO Fix this to work with string_view

    std::string request {m_method.data()};
    ((request += " ") += m_path.data()) += "?";
    for(const auto& it : m_query_params)
        (((request += it.first.data()) += "=") += it.second.data()) += "&";

    request.pop_back();
    // TODO append fragment to headerline (#...)
    request += " HTTP/1.1\r\n";
   
    for(const auto& it : m_headers)
        (((request += it.first.data()) += ": ") += it.second.data()) += "\r\n";

    request += "\r\n";

    for(const auto& it : m_body_params)
        (((request += it.first.data()) += "=") += it.second.data()) += "&";

    request.pop_back();
    
    return request;
}

cookie request::get_cookie(const std::string& cookie_name) const
{
    try
    {
        return m_cookies.at(cookie_name);
    }
    catch(std::out_of_range& e)
    {
        throw std::out_of_range {"No matching cookie found"};
    }
}

void request::parse_requestline(std::string_view requestline)
{
    uint32_t last_index = 0, vec_index = 0;
    std::string_view tmp_store[3];
    for(uint32_t i = 0; i < requestline.size() && vec_index < 3; i++)
    {
        if(requestline[i] == ' ' || i + 1 == requestline.size())
        {
            tmp_store[vec_index] = std::string_view {requestline.data() + last_index, i - last_index};
            last_index = i + 1;
            vec_index++;
        }
    }

    if(vec_index != 3)
        throw std::invalid_argument {"invalid_requestline"};

    this->m_method = tmp_store[0];
    this->m_resource = tmp_store[1];
    this->m_protocol = tmp_store[2];
}

void request::parse_params(std::string_view param_string, std::map<std::string_view, std::string_view>& param_container)
{
    uint32_t offset = 0;
    for(uint32_t i = 0; i <= param_string.length(); i++)
    {
        if(param_string[i] == '&' || param_string[i] == '#' || i == param_string.length())
        {
           std::string_view param {param_string.data() + offset, i - offset};
            size_t pos = param_string.find('=');
            if(pos != std::string::npos)
            {
                param_container.insert({ std::string_view {param.data(), pos}, 
                    std::string_view {param.data() + pos + 1, i - offset - pos - 1} });
            }
            offset = i + 1;
        }
    }
}

void request::parse_cookies(const std::string& cookies)
{
    std::istringstream iss {cookies};
    std::vector<std::string> cookies_split {(std::istream_iterator<std::string> {iss}),
                                          std::istream_iterator<std::string> {}};

    for(auto& it : cookies_split)
    {
        if(it.find(';') != string::npos)
            it = it.substr(0, it.length() - 1);

        size_t pos = it.find('=');
        cookie c(it.substr(0, pos), it.substr(pos + 1));
        m_cookies.insert(std::pair<std::string, cookie> {it.substr(0, pos), c});
    }

}

bool request::check_header(const std::string& key) const
{
    auto it = m_headers.find(key);
    if(it == m_headers.end())
        return false;
    else
        return true;
}

std::string request::get_header(const std::string& key) const
{
    try {
        return std::string {m_headers.at(key)};
    } catch(std::out_of_range& e) {
        return "";
    }
}

std::string request::get_param(const std::string& key) const
{
    try {
        return std::string {m_query_params.at(key)};
    } catch(std::out_of_range& e) {
        return "";
    }
}

std::string request::get_post_param(const std::string& key) const
{
    try {
        return std::string {m_body_params.at(key)};
    } catch(std::out_of_range& e) {
        return "";
    }
}

} // namespace http
