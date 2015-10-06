#include <stdio.h>
#include <stdlib.h>

#include "ptlcalls.h"
#include "util.h"


int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Error: Need at least 1 argument.\n");
		abort();
	}

	uint64_t count;
	string count_str(argv[1]);
	convert_uint64_t(count, count_str, "first argument");

	for (int i=0; i<count; i++)
	{
		ptlcall_hybridsim(0, i);
	}
}
