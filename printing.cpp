#include "printing.h"
#include "interrupt_protocol.h"

#if SOCKET_BUILD
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <cstdio>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "debug.h"

#include <sys/types.h>
struct Socket{
		int socketfd;
		int clientfd;
		int eventfd;
		bool clientConnected;
		bool socketCreated;
};

struct Socket sock = {-1, -1, -1, false, false};

char sockBuffer[255];
socklen_t clilen;
struct sockaddr_in serv_addr;
struct sockaddr_in cli_addr;
// int n; // Needed for read

FILE *outFile = nullptr;
FILE *evFile = nullptr;

void error(const char *msg)
{
		printf("%s", msg);
		exit(1);
}

void openFile(){
		outFile = fopen(PRINT_OUT, "a");
		if(outFile == NULL)
				error("File could not be opened\n");
		evFile = fopen(PRINT_EVT, "a");
		if(evFile == NULL)
				error("Event File could not be opened\n");
}

void closeClient(){
		close(sock.clientfd);
		close(sock.eventfd);
}

void Printing::closeConnection(){
	close(sock.socketfd);
	if (outFile != nullptr){
		fclose(outFile);
		outFile = nullptr;
		evFile = nullptr;
	}
}

void initSocket(int portno){
		debug("initializing socket\n");
		 sock.socketfd =  socket(AF_INET, SOCK_STREAM, 0);
		 if (sock.socketfd < 0) 
				error("ERROR opening socket\n");


		 bzero((char *) &serv_addr, sizeof(serv_addr));

		 serv_addr.sin_family = AF_INET;  
		 serv_addr.sin_addr.s_addr = INADDR_ANY;  
		 serv_addr.sin_port = htons(portno);

		 debug("prior binding socket\n");
		 if (bind(sock.socketfd, (struct sockaddr *) &serv_addr,
							sizeof(serv_addr)) < 0) 
							error("ERROR on binding");

		 listen(sock.socketfd,5);

		 clilen = sizeof(cli_addr);
		 sock.socketCreated = true;
}

void Printing::setupConnection(int portno) {
		debug("setting up socket at port %d\n", portno);

		//establishing a socket connection
		if (!sock.socketCreated)
				initSocket(portno);

		if(sock.clientConnected)
				closeConnection();

		if(outFile == nullptr)
				openFile();

		 sock.clientfd = accept(sock.socketfd, 
								 (struct sockaddr *) &cli_addr, &clilen);
		 if (sock.clientfd  < 0) 
					error("ERROR on accept");

		 sock.eventfd = accept(sock.socketfd, 
								 (struct sockaddr *) &cli_addr, &clilen);

		 if (sock.eventfd  < 0) 
					error("ERROR on accept of second socket");
		 sock.clientConnected = true;

		 debug("server: got connection from %s port %d\n",
						inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));


		 send(sock.clientfd, "connected!", 10, 0);
		 send(sock.eventfd, "foratbp!", 8, 0);

		 bzero(sockBuffer,256);
}

void sendFile(const char *FILE_NAME, FILE *f, int sockfd){
		debug("sending the file through socket\n");
		fflush(f);

		int fd = open(FILE_NAME, O_RDONLY);
		struct stat file_stat;
		char file_size[256] = {0};
		ssize_t len ;
		int remain_data;
		off_t offset;
		ssize_t sent_bytes = 0;

		if (fd == -1)
				error("Error opening file -->");

		if(fstat(fd, &file_stat) < 0)
				error("fstat error -->\n");
		sprintf(file_size, "%ld", file_stat.st_size);

		len = send(sockfd, file_size, sizeof(file_size), 0);
		if (len < 0) 
				error("Error on sending");

		offset = 0;
		remain_data = file_stat.st_size;
		while (((sent_bytes = sendfile(sockfd, fd, &offset, BUFSIZ)) > 0)
						&& (remain_data > 0)){
				remain_data -= sent_bytes;
		}

		close(fd);

		f = freopen(FILE_NAME, "w", f);

		f = freopen(FILE_NAME, "a", f);
}

void Printing::printo(const char *format, ...){
		va_list args;
		va_start(args, format);
		vfprintf(outFile, format, args);
		va_end(args);
		if(!this->longPrint)
				sendFile((const char *) PRINT_OUT, outFile, sock.clientfd);
}
void Printing::printevent(const char *format, ...){
		va_list args;
		va_start(args, format);
		vfprintf(evFile, format, args);
		va_end(args);
		sendFile((const char*) PRINT_EVT, evFile, sock.eventfd);
}

void Printing::printRaw(uint32_t total_elems, uint32_t bytes_per_elem, char* elems){
		sendFile((const char *) PRINT_OUT, outFile, sock.clientfd);
		outFile = freopen(PRINT_OUT, "wb", outFile);
		fwrite((void *) &total_elems, sizeof(uint32_t), 1, outFile);
		fwrite((void *) &bytes_per_elem, sizeof(uint32_t),1, outFile);
		fwrite((void *) elems, sizeof(char), total_elems * bytes_per_elem, outFile);
		sendFile((const char *) PRINT_OUT, outFile, sock.clientfd);
}

#else

#include <stdarg.h>
#include "Arduino.h"

void Printing::printo(const char *format, ...){
		va_list argptr;
		va_start(argptr, format);
		vprintf(format, argptr);
		va_end(argptr);
}

void Printing::printevent(const char *format, ...){
		va_list argptr;
		va_start(argptr, format);
		vprintf(format, argptr);
		va_end(argptr);
}

void Printing::flush(){}
void Printing::setupConnection(int port){}
void Printing::closeConnection(){}

void Printing::printRaw(uint32_t total_elems, uint32_t bytes_per_elem, char* elems){
	printf(DUMP_BYTES);
	Serial.write((byte *) &total_elems, sizeof(uint32_t));
	Serial.write((byte *) &bytes_per_elem, sizeof(uint32_t));
	Serial.write((byte *) elems, total_elems * bytes_per_elem);
	printf(DUMP_BYTES_END);
}

#endif

