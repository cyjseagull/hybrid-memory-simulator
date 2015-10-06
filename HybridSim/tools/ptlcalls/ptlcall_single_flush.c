#include <stdio.h>
#include <stdlib.h>

#include "ptlcalls.h"


int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Error: Need exactly 1 argument.\n");
		abort();
	}

	ptlcall_single_flush(argv[1]);
}
