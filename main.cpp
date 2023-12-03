#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "SSParser.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

// Extracting SEI messages from an H.264 data stream
void extractSEIMessages(const uint8_t* nalUnit, size_t length)
{
    // Checking for SEI messages
    if (nalUnit[0] == 0x06 && nalUnit[1] == 0x05)
    {
        // Extracting a timestamp
        uint32_t timestamp = (nalUnit[2] << 24) | (nalUnit[3] << 16) | (nalUnit[4] << 8) | nalUnit[5];
        std::cout << "Timestamp: " << timestamp << std::endl;
    }
}

// H.264 Data Stream processing
void processH264Stream(const uint8_t* stream, size_t length)
{
    // Search for NAL units
    for (size_t i = 0; i < length; )
    {
        // Search for the beginning of the NAL unit
        size_t startCodeLength = 0;
        while (i + startCodeLength < length && !(startCodeLength == 2 && stream[i + startCodeLength] == 0x03))
            startCodeLength += (startCodeLength < 2) ? 1 : 0;

        // Search for the end of the NAL unit
        size_t end = i + startCodeLength;
        while (end < length && !(end + 2 < length && stream[end] == 0x00 && stream[end + 1] == 0x00 && stream[end + 2] == 0x01))
            end++;

        // Extracting the NAL unit
        const uint8_t* nalUnit = &stream[i + startCodeLength];
        size_t nalUnitLength = end - i - startCodeLength;

        // Extracting SEI messages from the NAL unit
        extractSEIMessages(nalUnit, nalUnitLength);

        // Move to the next NAL unit
        i = end;
    }
}

std::string extract_boundary(SSParser parser) {
    parser.to("Content-Type:");
    parser.to("boundary=");
    const auto boundary = parser.extract("boundary=", "\r\n").to_string();
    return "\r\n--" + boundary;
}

net::awaitable<void> start(std::string host, std::string port, std::string path, std::string body = {}) {
    auto executor = co_await net::this_coro::executor;
    tcp::resolver resolver(executor);
    beast::tcp_stream stream(executor);

    // Look up the domain name
    auto const results = co_await resolver.async_resolve(host, port, net::use_awaitable);

    // Set a timeout on the operation
    stream.expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    co_await stream.async_connect(results, net::use_awaitable);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{ http::verb::get, path, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.body() = body;
    req.prepare_payload();

    // Send the HTTP request to the remote host
    co_await http::async_write(stream, req, net::use_awaitable);
    beast::flat_buffer buffer;
    std::fstream file(R"(C:\Users\irahm\Documents\fileout1.txt)", std::ios::out | std::ios::binary);

    auto async_read = [&]() -> net::awaitable<SSParser> {
        const size_t bytes_transferred = co_await stream.socket().async_read_some(buffer.prepare(4096), net::use_awaitable);
        buffer.commit(bytes_transferred);
        auto data = buffer.data();
        co_return SSParser{{static_cast<const char*>(data.data()), data.size()}};
    };
    SSParser response = co_await async_read();
    const std::string boundary = extract_boundary(response);
    if (boundary.empty()) {
        std::cerr << "Error: can't find boundary name" << std::endl;
        std::cerr << response;
        co_return;
    }

    bool result = response.to(boundary);
    if (!result) {
        std::cerr << "Error: can't find boundary" << std::endl;
        co_return;
    }

    size_t content_length = 0;

    try {
        do {
            if (content_length != 0) {
                auto payload = response.extract(content_length);
                response.skip(payload.size());
                file << payload;
                //processH264Stream(reinterpret_cast<const uint8_t*>(payload.to_string_view().data()), payload.size());
                if (payload.size() <= content_length) {
                    content_length -= payload.size();
                } else {
                    content_length = 0;
                    std::cerr << "Warning: payload is more than content length" << std::endl;
                }
                buffer.consume(response.get_original_position());
                response = co_await async_read();
                continue;
            }
            result = response.to("Content-Length: ");
            if (!result) {
                std::cerr << "Error: can't find Content-length." << std::endl;
                break;
            }

            content_length = response.take_uint64();
            if (!response.expect("\r\n\r\n")) {
                std::cerr << "Warning: There is no any CLRF after Content-Length." << std::endl;
                break;
            }
            static size_t i = 0;
            static size_t fps = 21;
            std::cout << (i++ / fps) << ' ' << content_length << '\n';
            if (content_length == 0) {
                std::cerr << "Error: the content length is zero";
                break;
            }

            auto payload = response.extract(content_length);
            response.skip(payload.size());
            //processH264Stream(reinterpret_cast<const uint8_t*>(payload.to_string_view().data()), payload.size());
            file << payload;

            if (payload.size() <= content_length) {
                content_length -= payload.size();
            } else {
                content_length = 0;
                std::cerr << "Warning: payload is more than content length" << std::endl;
            }
            buffer.consume(response.get_original_position());

            response = co_await async_read();
        } while (stream.socket().is_open());
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Gracefully close the socket
    stream.close();
    file.close();
}

int main(int argc, char* argv[]) {
    try {
        std::string url = "/cgi-bin/record.cgi?userName=admin&password=admin&action=playBack&cameraID=1&startTime=20230913044228&endTime=20230918120805";
        net::io_context ioc;
        net::co_spawn(ioc, start("115.160.173.206", "84", url), net::detached);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}