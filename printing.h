#ifndef PRINTING_H
#define PRINTING_H

#include <inttypes.h>

#if SOCKET_BUILD
#define PRINT_OUT "/tmp/warduino_out"
#define PRINT_EVT "/tmp/warduino_event"
#endif

class Printing {
		public:
				bool longPrint = false;
				void printo(const char *format, ...);
				void flush();
				void setupConnection(int portno);
				void closeConnection();
				void printRaw(uint32_t total_elems, uint32_t bytes_per_elem, char* elems);
				void printevent(const char *format, ...);
};

#endif
