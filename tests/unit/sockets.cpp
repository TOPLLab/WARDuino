#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/Utils/sockets.h"

#include <arpa/inet.h>
#include <doctest/doctest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <thread>

namespace {

int reserve_port() {
    const int descriptor = createSocketFileDescriptor();
    if (descriptor < 0) return -1;

    sockaddr_in address = createServerAddress(0);
    if (bindSocketToAddress(descriptor, address) < 0) return -1;

    socklen_t size = sizeof(address);
    if (getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size) <
        0) {
        ::close(descriptor);
        return -1;
    }
    ::close(descriptor);
    return ntohs(address.sin_port);
}

int connect_to(const int port) {
    const sockaddr_in address = createLocalhostAddress(port);
    for (int attempt = 0; attempt < 100; ++attempt) {
        const int descriptor = createSocketFileDescriptor();
        if (descriptor >= 0 &&
            connect(descriptor, reinterpret_cast<const sockaddr *>(&address),
                    sizeof(address)) == 0) {
            return descriptor;
        }
        if (descriptor >= 0) ::close(descriptor);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return -1;
}

}  // namespace

TEST_CASE("WebSocket closes the accepted client and listener") {
    const int port = reserve_port();
    REQUIRE(port > 0);

    WebSocket server(port);
    std::thread accept([&server] { server.open(); });
    const int client = connect_to(port);
    REQUIRE(client >= 0);
    accept.join();

    ::close(client);  // EOF must be observable by the communication loop.
    uint8_t byte = 0;
    CHECK(server.read(&byte, sizeof(byte)) == 0);

    server.close();
    server.close();

    const int replacement = createSocketFileDescriptor();
    REQUIRE(replacement >= 0);
    CHECK(bindSocketToAddress(replacement, createServerAddress(port)) == 0);
    ::close(replacement);
}
