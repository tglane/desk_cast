#include <http/webserver.hpp>
#include <http/request.hpp>
#include <http/response.hpp>

#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <charconv>

#include <iostream>

namespace http
{

static std::vector<char> file_to_binary(std::string_view path)
{
    std::ifstream ifs(path.data(), std::ios::binary);

    return std::vector<char>((std::istreambuf_iterator<char>(ifs)),
        (std::istreambuf_iterator<char>()));
}

static void handle_connection(std::unique_ptr<socketwrapper::TCPSocket>&& conn)
{
    // TODO improve connection handling

    size_t wait_cnt = 0;
    while(!conn->bytes_available())
    {
        if(wait_cnt >= 5)
            return;

        wait_cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    request req;
    try {
        req.parse(conn->read_all<char>().get());
    } catch(...) {
        response res;
        res.set_code(400, "Bad Request");
        std::string res_str = res.to_string();
        conn->write<char>(res_str.data(), res_str.size());
        return;
    }

    // Read file
    std::vector<char> img = file_to_binary("./test_data/test_video.mp4");

    response res(req);
    res.set_header("Server", "localhost");
    res.set_header("Content-Type", "video/mp4");

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
        res.set_body({img.begin() + start, img.begin() + end});
    }
    else
    {
        res.set_code(200);
        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Content-Length", std::to_string(img.size()));
        // res.set_body({img.begin(), img.end()});
    }
    
    // CORS
    if(req.check_header("Origin"))
    {
        res.set_header("Access-Control-Allow-Origin", req.get_header("Origin"));
        res.set_header("Access-Control-Allow-Methods", "GET");
    }

    std::string res_str = res.to_string();
    conn->write<char>(res_str.data(), res_str.size());
}

webserver::webserver(int32_t port, const char* cert_path, const char* key_path)
    : m_sock(AF_INET)
{
    m_sock.bind("0.0.0.0", port);
}

void webserver::serve(std::atomic<bool>& run_condition)
{
    m_sock.listen(1);

    std::cout << "Webserver serving ...\n";
    while(run_condition)
    {
        try {
            // Because this webserver only serves one client, here we dont need
            // more than one thread handling requests at the moment

            // TODO parse req here and go to correct route handler

            handle_connection(m_sock.accept());
        } catch(socketwrapper::SocketAcceptingException&) {}
    }

    std::cout << "Webserver closing" << std::endl;
}

} // namespace http
