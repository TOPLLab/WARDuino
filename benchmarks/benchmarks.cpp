/* 
 __      __                  .___    .__               
/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
 \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
       \/        \/           \/             \/        
       
WARDuino (c) by Christophe Scholliers & Robbert Gurdeep Singh 

WARDuino is licensed under a
Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this
work. If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
*/

#include "../WARDuino.h"
#include <iostream>
#include <string.h>

#define MAIN "_main"
#define MAX_PATH 100
#define MAX_BYTE_CODE_SIZE 10000
#define BENCHMARK_PATH "./"
#define append(buf, a, b) snprintf(buf, sizeof(buf), "%s%s", a, b)
#define array_size(x) (sizeof(x) / sizeof((x)[0]))

#define FOR_EACH(type, item, array)                    \
	{                                                  \
		for (size_t i = 0; i < array_size(array); i++) \
		{                                              \
			type item = array[i];
#define END \
	}       \
	}

typedef char *string;

void set_path(string path, string name)
{
	strncpy(path, BENCHMARK_PATH, array_size(path));
	strncat(path, name, array_size(path));
	strncat(path, "/wast/", array_size(path));
	strncat(path, name, array_size(path));
	strncat(path, ".wasm", array_size(path));
}

unsigned int read_file_to_buf(unsigned char *bytes, string path)
{
	FILE *file = fopen(path, "r");
	if (file == NULL)
	{
		fprintf(stderr, "Cannot open file: %s", path);
		exit(1);
	}
	fseek(file, 0L, SEEK_END);
	long num_bytes = ftell(file);
	if (num_bytes < MAX_BYTE_CODE_SIZE)
	{
		fseek(file, 0L, SEEK_SET);
		long result = fread(bytes, sizeof(char), num_bytes, file);
		if (result != num_bytes)
		{
			fprintf(stderr, "reading error while loading file %s", path);
			exit(1);
		}
		fclose(file);
		return (unsigned int)num_bytes;
	}
	else
	{
		fprintf(stderr, "File  < %s  > is too big for buffer", path);
		exit(1);
	}
}

void run_benchmarks(string benchmarks[])
{
	char path[MAX_PATH];
	unsigned char bytes[MAX_BYTE_CODE_SIZE];
	unsigned int bytes_length;
	WARDuino *w = new WARDuino();
	FOR_EACH(char *, name, benchmarks)
		set_path(path, name);
		printf("Starting test %s \n", path);
		bytes_length = read_file_to_buf(bytes, path);
		Options opt;
		Module* m = w->load_module(bytes, bytes_length,opt);
		int fidx  = w->get_export_fidx(m,MAIN);
		w->invoke(m,fidx);
	END
}

int main(int argc, const char *argv[])
{
	string benchmarks[] = {"tak"};
	run_benchmarks(benchmarks);
	return 0;
}
