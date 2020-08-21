#include <small/cpbuf.hpp>
#include <time.h>
#include "unit.h"


enum {
	/* Each data chunk must be less than 16kB. */
	SLAB_SIZE = 16 * 1024 * 1024
};

int main()
{
	DefaultAllocator<100> allocator();

	printf("Allocator created!");

}
