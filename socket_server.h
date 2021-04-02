#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <inttypes.h>

/*
socker_server is a small server that manages socket connections established with
the device. The server allows clients to specify the purpose of the established
connection by means of a one byte bitmap called socketflags (see below). For
example socketflags 0x03 tells the server to use the a socket as the new stdin
and stdout.

If at any point any socket connection dies, the server also ensures for the
needed cleanup. The cleanup involes the closing of a socket descriptor as-well
as the restore of the original stdin, stdout and stderr (if needed). Note that
two different sockets cannot be used for the same flags. For example, two
sockets flagged as stdout results in the prior being closed.

*/

/*
        Server constants configurations - ideally set according to each
   microcontroller
*/
#define SOCK_SENDBUF_SIZE 250
#define SOCK_RECVBUF_SIZE 250
#define SOCK_BACKLOG_SIZE 5

/*
        socketflags is a one byte bitmap where each bit tell the purpose of the
   socket
        - If a socket is configured with a FLAG_IN bit the socket is checked
   periodically for any incoming data. The read data can be retrieved via
   getReceivedData()
        - socket flagged as FLAG_DBG can be used to redirect any debug prints
        - socket flagged as FLAG_EVT can be used to redirect events e.g. when
   WARDuino warns of reached a breakpoits and outputs 'At breakpoint'

*/

#define FLAG_IN 0x01   // socket as stdin
#define FLAG_OUT 0x02  // socket as stdout
#define FLAG_ERR 0x04  // socket as stderr
#define FLAG_DBG 0x08  // socket as only dbg
#define FLAG_EVT 0x10  // socket as events
#define FLAG_MAX 0x1f  // socket used for all above

typedef uint8_t socketflags;

/*
initializes socket server at host and port number.
*/
void initializeServer(const char* host, int portno);

/*
Retrieves the socketdescriptor configured for `flag` use.
returns -1 if none is present
*/
int getSocketDescriptor(socketflags flag);

/*
process all incoming events for all registered sockets
*/
void processIncomingEvents();

/*
returns a pointer to char if data has been read by a FLAG_IN configured socket.
If no data has been read a nullptr is returned
*/
char* getReceivedData();

/*
returns the size of getReceivedData();
*/
uint32_t receivedDataSize();

/*
frees the received data
*/
void freeReceivedData();

void socket_debug(const char* format, ...);  // TODO remove

#endif