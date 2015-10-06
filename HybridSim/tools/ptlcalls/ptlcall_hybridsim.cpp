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

	uint64_t operation = 0;
	uint64_t address = 0;

	string op_str(argv[1]);
	convert_uint64_t(operation, op_str, "first argument");

	if (argc > 2)
	{
		string address_str(argv[2]);
		convert_uint64_t(address, address_str, "second argument");
	}

	ptlcall_hybridsim(operation, address);
}
