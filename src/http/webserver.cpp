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
    // -> Check if request is from cast device
    // -> Check if it points to a valid endpoint

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
    } catch(socketwrapper::SocketReadException&) {
        return;
    }
    std::cout << req.to_string() << std::endl;

    std::vector<char> img = file_to_binary("./test_data/test_video.mp4");
    std::cout << "File size: " << img.size() << std::endl;

    response res(req);
    res.set_code(200);
    res.set_header("Server", "localhost");
    res.set_header("Content-type", "video/mp4");
    res.set_body({img.begin(), img.end()});

    // std::string res("HTTP/1.1 200 OK\r\nServer: localhost\r\nContent-length: ");
    // res.append(std::to_string(img.size()));
    // res.append("\r\nContent-type: video/mp4\r\n\r\n");
    // res.append({img.begin(), img.end()});

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
