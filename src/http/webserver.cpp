#include "http/webserver.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <fstream>
#include <atomic>
#include <charconv>

#include <iostream>

namespace http
{

static std::vector<char> file_to_binary(std::string_view path)
{
    std::ifstream ifs(path.data(), std::ios::binary);

    return std::vector<char> {(std::istreambuf_iterator<char>(ifs)),
        (std::istreambuf_iterator<char>())};
}

static void serve_hls_stream(net::tcp_connection<net::ip_version::v4>&& conn)
{
    request req;
    try {
        std::array<char, 4096> buffer;
        conn.read(net::span {buffer});
        req.parse(buffer.data());
    } catch(...) {
        response res;
        res.set_code(400, "Bad Request");
        conn.send(net::span {res.to_string()});
        return;
    }

    // std::cout << "[DEBUG]:\n" << req.to_string() << std::endl;

    std::string_view pv = req.get_path();
    std::string path;
    path.reserve(11 + pv.size());
    path.assign("./test_data");
    std::copy(pv.begin(), pv.end(), std::back_inserter(path));

    // Read file
    std::vector<char> img = file_to_binary(path.data());
    if(img.size() <= 0) 
    {
        response res;
        res.set_code(400, "Bad Request");
        std::string res_str = res.to_string();
        conn.send(net::span {res.to_string()});
        return;
    }

    response res {req};
    res.set_header("Server", "localhost");
    res.set_header("Content-Type", "application/x-mpegurl");

    if(req.check_header("Range"))
    {
        size_t start = 0, end = img.size();
        std::string_view hdr = req.get_header("Range");
        size_t hyphen_pos = hdr.find("-");

        if(hdr.find("bytes=") != std::string::npos && hyphen_pos != std::string::npos)
        {
            std::from_chars(hdr.data() + 6, hdr.data() + hyphen_pos, start);
            std::from_chars(hdr.data() + hyphen_pos + 1, hdr.data() + hdr.size(), end);
        }

        res.set_code(206, "Partial Content");
        res.set_header("Content-Range", "bytes " + std::to_string(start) + '-' + 
            std::to_string(end - start - 1) + '/' + std::to_string(img.size()));
        res.set_body(std::string {img.begin() + start, img.begin() + end});
    }
    else
    {
        res.set_code(200);
        res.set_header("Content-Length", std::to_string(img.size()));
        res.set_body(std::string {img.begin(), img.end()});
    }
    
    // CORS
    res.set_header("Accept-Ranges", "bytes");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "OPTIONS, GET, HEAD");

    conn.send(net::span {res.to_string()});
}

void webserver::serve(std::atomic<bool>& run_condition)
{
    std::cout << "Webserver serving ..." << std::endl;
    while(run_condition.load())
    {
        try {
            // Because this webserver only serves one client, here we dont need
            // more than one thread handling requests at the moment

            // TODO Parse request here and go to correct service function when supoorting multiple protocols

            serve_hls_stream(m_acceptor.accept());
        } catch(std::runtime_error&) {}
    }

    std::cout << "Webserver closing" << std::endl;
}

} // namespace http

