#ifndef _MEMORY_CONTROLLER_BUILDER_H
#define _MEMORY_CONTROLLER_BUILDER_H

#include <machine.h>
#include <memoryController.h>

#include <NVMainMemory.h>

#include <string>
#include "stdlib.h"
#include <cassert>
namespace Memory
{
	struct MemoryControllerBuilder : public ControllerBuilder
	{
	    //default memory type is DRAM	
		MemoryControllerBuilder(const char* name ) : ControllerBuilder(name)
		{
		}
		
		Controller* get_new_controller(W8 coreid, W8 type,
									MemoryHierarchy& mem, const char *name)
		{
			#ifdef NVMAIN
			return new NVMainMemory(coreid, name, &mem);
			#endif
			return new MemoryController(coreid, name, &mem);
		}
	};
};
#endif
