#include "radio.h"

#if defined(RADIO_UDP)

#include <cstring>
#include <cstdio>
#include <cstdlib>
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> //sleep
#include <fcntl.h> //socket options
#include <cerrno>
 
// -------------------------------------------------
//        Mock replacement using UDP Sockets
// -------------------------------------------------

//default port number
#ifndef NODE_PORT
#define NODE_PORT 5001
#endif
 
// All ports in the simulation
static uint16_t    PEER_PORTS[]     = {5001, 5002, 5003, 5004};
static uint16_t    PEER_COUNT       = 4;
static const char* LOCALHOST        = "127.0.0.1";
static uint16_t    TIMEOUT_MS       = 5000;
static uint16_t    POLL_INTERVAL_MS = 50'000;
 
static int SOCKET = -1; // UDP socket, bound to NODE_PORT
 
extern "C" {
 
uint16_t radio_begin_extern(uint32_t freq, uint32_t bw, uint32_t sf, uint32_t cr, uint32_t sw, uint32_t pwr) {
    if (SOCKET >= 0) return 0; // socket already established
 
    //socket creation
    SOCKET = socket(AF_INET, SOCK_DGRAM, 0);
    if (SOCKET < 0) { 
        perror("UDP begin: socket creation failed"); 
        exit(1); 
    }
 
    //socket option: makes binding multiple processes to the same port possible
    int flag = 1;
    setsockopt(SOCKET, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag));
 
    //non-blocking socket = poll forever without getting stuck
    int flags = fcntl(SOCKET, F_GETFL, 0);
    fcntl(SOCKET, F_SETFL, flags | O_NONBLOCK);
 
    //tells the socket where to send data to 
    sockaddr_in addr{};
    addr.sin_family = AF_INET; //IPv4 address
    addr.sin_port = htons(NODE_PORT); //port number
    inet_pton(AF_INET, LOCALHOST, &addr.sin_addr); //listen to localhost
 
    //tells OS to route messages over port 5001 to this process
    if (bind(SOCKET, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP begin: bind failed"); 
        exit(1);
    }

    return 0;
}
 
//broadcast message to every peer except own socket.
uint16_t radio_transmit_bytes_extern(uint8_t* data, size_t length) {
    if (SOCKET < 0) {
        perror("UDP transmit: socket not initialised"); 
        exit(1);
    };
 
    sockaddr_in dest{};
    dest.sin_family = AF_INET; //IPv4 address
    inet_pton(AF_INET, LOCALHOST, &dest.sin_addr); //listen to localhost
 
    for (int i = 0; i < PEER_COUNT; i++) {
        if (PEER_PORTS[i] == NODE_PORT) continue; // skip own socket
 
        dest.sin_port = htons(PEER_PORTS[i]); //send to right port
        ssize_t sent = sendto(SOCKET, data, length, 0, (sockaddr*)&dest, sizeof(dest));

        if (sent < 0) {
            //no ressources found
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(POLL_INTERVAL_MS);
                continue;
            }
            perror("UDP transmit: sendto failed");
            return 1;
        }
    }
    return 0;
}
 
// Block-poll until a datagram arrives or the timeout expires.
uint16_t radio_receive_bytes_extern(uint8_t* data, size_t length) {
    if (SOCKET < 0) {
        perror("UDP receive: socket not initialised"); 
        exit(1);
    };
 
    //amount of times receive will poll information, not while true in case of non arriving data
    int iterations = (TIMEOUT_MS * 1000) / POLL_INTERVAL_MS;
 
    sockaddr_in sender{};
    socklen_t sender_length = sizeof(sender);
 
    for (int i = 0; i < iterations; i++) {
        ssize_t n = recvfrom(SOCKET, data, length, 0, (sockaddr*)&sender, &sender_length);

        if (n > 0) {
            return 0; //success
        }
        if (n < 0) {
            //no ressources found
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(POLL_INTERVAL_MS);
                continue;
            }
            perror("UDP receive: recvfrom failed");
            return 1; //failure
        }
        usleep(POLL_INTERVAL_MS);
    }
 
    printf("UDP receive: timeout after %d ms\n", TIMEOUT_MS);
    return 1;
}

uint16_t radio_startReceive_extern() {
    return 0; // Not implemented
}
 
}

#endif //RADIO_UDP