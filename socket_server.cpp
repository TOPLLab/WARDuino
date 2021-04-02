#include "socket_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

// temporary imports
#include <errno.h>   //TODO remove
#include <stdarg.h>  //TODO remove
#include <stdlib.h>  //TODO remove

#include <cstdio>  //TODO remove

#define MAX_SOCKETS 8

/*
private functions
*/
void _errorexit(const char *);
void _closeSocket(int);
bool _flagSocket(int, socketflags);
int _unflaggedPos(int);
void _removeFlag(struct ClientSocket *, socketflags);
struct ClientSocket *_findSocket(socketflags);
void _fillPollfds();
void _resizeArrays();
int _fdfindSocketPos(int);
int _findSocketPos(socketflags);
void _restoreStds(socketflags);

void _printArrays();                         // TODO remov
void _errorexitnr(const char *, int);        // TODO remove
void socket_debug(const char *format, ...);  // TODO remove

/*
StdInOuts keeps track of the original stdin, stdout & stderr
*/
struct StdInOuts {
  int in;
  int out;
  int err;
} stds = {dup(fileno(stdin)), dup(fileno(stdout)), dup(fileno(stderr))};

// socket for accepting new sockets
struct ServerSocket {
  int fd;
  struct sockaddr_in addr;
} server;

// struct keeps track of the input socket fd, and received content in buff
// buff is never nullified, size represents the content that can be processed
struct InputSocket {
  int fd;
  char *buff;
  const uint32_t max_size = SOCK_RECVBUF_SIZE;
  uint32_t size;
} inputsocket;

// accepted sockets
struct ClientSocket {
  int fd;
  socketflags flags;
};

struct ClientSocket sockets[MAX_SOCKETS];
short int nsockets = 0;

// accepted sockets not yet flagged
int unflagged_fds[MAX_SOCKETS];
short int unflag_size = 0;

// array used to poll
struct pollfd fds[MAX_SOCKETS];
short int nfds = 0;

int getFileDescriptor(socketflags flags) {
  struct ClientSocket *sock = _findSocket(flags);
  if (sock == nullptr) return -1;
  return sock->fd;
}

char *getReceivedData() {
  if (inputsocket.size > 0)
    return inputsocket.buff;
  else
    return nullptr;
}

uint32_t receivedDataSize() {
  if (inputsocket.size > 0)
    return inputsocket.size;
  else
    return 0;
}

void freeReceivedData() { inputsocket.size = 0; }

void initializeServer(const char *host, int portno) {
  socket_debug("inializing socket at host %s - port %d\n", host, portno);

  int listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sd < 0) _errorexit("socket()\n");  // TODO use fatal?

  // make port reusable  & sockets non-blocking
  int enable = 1;
  if (setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable,
                 sizeof(enable)) < 0)
    _errorexit("setsockopt()\n");

  if (ioctl(listen_sd, FIONBIO, (char *)&enable) < 0)
    _errorexit("error on ioctl()\n");

  bzero((char *)&server.addr, sizeof(server.addr));

  // binding socket
  server.addr.sin_family = AF_INET;  // TODO use AF_INET6?
  server.addr.sin_addr.s_addr = INADDR_ANY;
  server.addr.sin_port = htons(portno);

  if (bind(listen_sd, (struct sockaddr *)&server.addr, sizeof(server.addr)) < 0)
    _errorexit("error on bind()\n");

  if (listen(listen_sd, SOCK_BACKLOG_SIZE) < 0)
    _errorexit("error on listen()\n");

  // init state
  server.fd = listen_sd;
  inputsocket.fd = -1;
  inputsocket.buff = (char *)malloc(inputsocket.max_size);
  memset(unflagged_fds, -1, sizeof(unflagged_fds));
  for (auto i = 0; i < MAX_SOCKETS; i++) sockets[i].fd = -1;
}

void processIncomingEvents() {
  _fillPollfds();

  int timeout = 300;  // ms
  int qevs = poll(fds, MAX_SOCKETS, timeout);
  if (qevs < 0)  // TODO handle propertly
    _errorexit("poll()\n");
  if (qevs == 0)  // timeout
    return;

  for (auto i = 0; i < nfds; i++) {
    // nothing happened
    if (fds[i].revents == 0) continue;

    // error occured
    if (fds[i].revents != POLLIN) {
      _closeSocket(fds[i].fd);
      socket_debug("error: fd=%d revents != POLLIN. Got %d\n", fds[i].fd,
                   fds[i].revents);
      continue;
    }

    if (fds[i].fd == server.fd) {
      // accept sockets
      int sd = -1;
      do {
        sd = accept(server.fd, 0, 0);
        if (sd < 0) {
          if (errno != EWOULDBLOCK) {
            socket_debug("WARNING CLOSING SOCKET SERVER fd=%d\n", server.fd);
            _closeSocket(server.fd);  // TODO restart listen fd
          }
          break;
        }
        // save socket as unflagged
        unflagged_fds[unflag_size++] = sd;
      } while (sd != -1);
    } else if ((inputsocket.fd != -1) && fds[i].fd == inputsocket.fd) {
      /* input socket is allowed to send data.
         i.e. the interrupt data consumed by WARDuino
      */
      int len = recv(fds[i].fd, inputsocket.buff, inputsocket.max_size, 0);
      if (len > 0) {
        inputsocket.size = len;
        socket_debug("received total #%" PRIu32 "\n", inputsocket.size);
        continue;
      }

      _closeSocket(fds[i].fd);
      socket_debug("closing input socket fd=%d\n", len, fds[i].fd);

    } else if (_unflaggedPos(fds[i].fd) >= 0) {
      /* non-flagged sockets are allowed to send data.
         i.e. one byte flags
      */

      socket_debug("receiving data from unflagged socket fd=%d\n", fds[i].fd);

      short int p = _unflaggedPos(fds[i].fd);
      unflagged_fds[p] = -1;

      socketflags flags;
      if (recv(fds[i].fd, &flags, sizeof(socketflags), 0) <= 0) {
        socket_debug("Issue in unflagged socket=%d (closing)\n", fds[i].fd);
        _closeSocket(fds[i].fd);
      }

      socket_debug("sending the received flags\n");
      if (send(fds[i].fd, &flags, sizeof(socketflags), 0) < 0) {
        _closeSocket(fds[i].fd);
        continue;
      }

      if (flags & FLAG_IN) {
        socket_debug("sending the receive buffer size\n");
        uint32_t max = SOCK_RECVBUF_SIZE;
        if (send(fds[i].fd, &max, sizeof(uint32_t), 0) <= 0)
          _closeSocket(fds[i].fd);
      }

      socket_debug("flagging socket=%d with %" PRIu8 "\n", fds[i].fd, flags);
      if (!_flagSocket(fds[i].fd, flags)) {
        _closeSocket(fds[i].fd);
        continue;
      }

    } else {
      /* All other sockets are not allowed to send data.
      We close them for safety
      */
      _closeSocket(fds[i].fd);
      socket_debug(
          "receiving data from socket=%d not supposed to send "
          "bytes\n",
          fds[i].fd);
    }
  }
  _resizeArrays();
  _printArrays();  // TODO remove
}

bool _flagSocket(int sockfd, socketflags flags) {
  if (flags > FLAG_MAX) return false;

  if (flags & FLAG_IN) {  // replace  stdin
    _removeFlag(_findSocket(FLAG_IN), FLAG_IN);
    dup2(sockfd, 0);
    inputsocket.fd = sockfd;
  }
  if (flags & FLAG_OUT) {  // replace  stdout
    _removeFlag(_findSocket(FLAG_OUT), FLAG_OUT);
    dup2(sockfd, fileno(stdout));
  }
  if (flags & FLAG_ERR) {  // replace stderr
    _removeFlag(_findSocket(FLAG_ERR), FLAG_ERR);
    dup2(sockfd, 2);
  }
  if (flags & FLAG_DBG)  // use for debug prints
    _removeFlag(_findSocket(FLAG_DBG), FLAG_DBG);
  if (flags & FLAG_EVT)  // use for events wa_evprintf
    _removeFlag(_findSocket(FLAG_EVT), FLAG_EVT);

  for (auto i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i].fd == -1) {
      sockets[i].fd = sockfd;
      sockets[i].flags = flags;
      break;
    }
  }

  return true;
}

void _removeFlag(struct ClientSocket *socket, socketflags flag) {
  if (socket == nullptr) return;

  // restore stdin, out & err
  _restoreStds(flag);

  if (flag & FLAG_IN) inputsocket.fd = -1;
  socket->flags &= ~flag;

  // close not longer needed socket
  if (socket->flags == 0) _closeSocket(socket->fd);
}

void _closeSocket(int fd) {
  close(fd);
  int p = _fdfindSocketPos(fd);
  if (p >= 0) {
    socketflags flags = sockets[p].flags;
    _restoreStds(flags);
    if (flags & FLAG_IN) inputsocket.fd = -1;
    sockets[p] = {-1, 0};
  } else if ((p = _unflaggedPos(fd)) >= 0)
    unflagged_fds[p] = -1;
  else {
    socket_debug("closing the server socket\n");
  }
}

void _restoreStds(socketflags flags) {
  if (flags & FLAG_IN) {
    dup2(stds.in, 0);
  }
  if (flags & FLAG_OUT) {
    dup2(stds.out, 1);
  }
  if (flags & FLAG_ERR) {
    dup2(stds.err, 2);
  }
}

int _unflaggedPos(int fd) {
  for (auto i = 0; i < MAX_SOCKETS; i++)
    if (unflagged_fds[i] == fd) return i;
  return -1;
}

int _findSocketPos(socketflags flags) {
  for (auto i = 0; i < nsockets; i++)
    if (sockets[i].flags & flags) return i;
  return -1;
}

int _fdfindSocketPos(int fd) {
  for (auto i = 0; i < nsockets; i++)
    if (sockets[i].fd == fd) return i;
  return -1;
}

struct ClientSocket *_findSocket(socketflags flags) {
  int index = _findSocketPos(flags);
  if (index < 0)
    return nullptr;
  else
    return sockets + index;
}

void _fillPollfds() {
  nfds = 0;
  fds[0].fd = server.fd;
  fds[0].events = POLLIN;
  nfds++;
  for (auto i = 0; i < nsockets; i++) {
    fds[nfds].fd = sockets[i].fd;
    fds[nfds].events = POLLIN;
    nfds++;
  }
  for (auto i = 0; i < unflag_size; i++) {
    fds[nfds].fd = unflagged_fds[i];
    fds[nfds].events = POLLIN;
    nfds++;
  }
  for (auto i = nfds; i < MAX_SOCKETS; i++) fds[i].fd = -1;
}

void _resizeArrays() {
  nsockets = 0;
  unflag_size = 0;
  int free_pos = 0;

  // resize sockets
  while (free_pos < MAX_SOCKETS) {
    if (sockets[free_pos].fd != -1) {
      nsockets++;
      free_pos++;
      continue;
    }

    int next = free_pos + 1;
    while (next < MAX_SOCKETS && (sockets[next].fd == -1)) next++;

    if (next >= MAX_SOCKETS) break;

    sockets[free_pos] = {sockets[next].fd, sockets[next].flags};
    sockets[next] = {-1, 0};
    nsockets++;
    free_pos++;
  }

  // resize unflagged_sockets
  free_pos = 0;
  while (free_pos < MAX_SOCKETS) {
    if (unflagged_fds[free_pos] != -1) {
      unflag_size++;
      free_pos++;
      continue;
    }

    short int next = free_pos + 1;
    while (next < MAX_SOCKETS && (unflagged_fds[next] == -1)) next++;

    if (next >= MAX_SOCKETS) break;

    unflagged_fds[free_pos] = unflagged_fds[next];
    unflagged_fds[next] = -1;
    unflag_size++;
  }
}

void _errorexit(const char *msg) {
  socket_debug("%s", msg);
  exit(-1);
}

void _errorexitnr(const char *msg, int nr) {
  socket_debug("%s %d\n", msg, nr);
  exit(-1);
}

// todo remove
void _printArrays() {
  socket_debug("listenfd fd=%d, inputsocket.fd=%d\n", server.fd,
               inputsocket.fd);
  for (auto i = 0; i < MAX_SOCKETS; i++) {
    socket_debug("socket %d: fp=%d, flags=%" PRIu8 "\n", i, sockets[i].fd,
                 sockets[i].flags);
  }
  for (auto i = 0; i < MAX_SOCKETS; i++) {
    socket_debug("unflag %d: fp=%d\n", i, unflagged_fds[i]);
  }
}

// todo remove
void socket_debug(const char *format, ...) {
  va_list argptr;
  va_start(argptr, format);
  int sockfd = getFileDescriptor(FLAG_DBG);

  if (sockfd != -1)
    vdprintf(sockfd, format, argptr);
  else {
    int sockout = getFileDescriptor(FLAG_OUT);
    if (sockout != -1)
      vdprintf(sockout, format, argptr);
    else {
      vdprintf(stds.out, format, argptr);
    }
  }
  va_end(argptr);
}