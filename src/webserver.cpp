#include <webserver.hpp>

#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>

#include <iostream>

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
    while(!conn->bytes_available() && wait_cnt <= 5)
    {
        wait_cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    std::unique_ptr<char[]> recv = conn->read_all<char>();
    std::cout << recv.get() << std::endl;

    std::vector<char> img = file_to_binary("./test_data/test_image.jpg");

    std::string res("HTTP/1.1 200 OK\r\nServer: localhost\r\nContent-length: ");
    res.append(std::to_string(img.size()));
    res.append("\r\n\r\n");
    res.append({img.begin(), img.end()});

    conn->write<char>(res.data(), res.size());

}

ssl_webserver::ssl_webserver(int32_t port, const char* cert_path, const char* key_path)
    : m_sock(AF_INET)
{
    m_sock.bind("0.0.0.0", port);
}

void ssl_webserver::serve(std::atomic<bool>& run_condition)
{
    m_sock.listen(1);

    std::cout << "Webserver serving ...\n";
    while(run_condition)
    {
        try {
            // Because this webserver only serves one client, here we dont need
            // more than one thread handling requests at the moment
            handle_connection(m_sock.accept());
        } catch(socketwrapper::SocketAcceptingException&) {}
    }

    std::cout << "Webserver closing" << std::endl;
}
