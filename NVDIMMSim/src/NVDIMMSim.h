#ifndef NVDIMMSIM_H
#define NVDIMMSIM_H
/*
 * This is a public header for NVDIMM including this along with libnvdimmsim.so should
 * provide all necessary functionality to talk to an external simulator
 */

#include "Callbacks.h"

#include <iostream>
#include <cstdlib>
#include <string>

#include <vector>
#include <queue>
#include <list>
#include <stdint.h>

using std::string;

namespace NVDSim
{
    typedef CallbackBase<void,uint64_t,uint64_t,uint64_t,bool> Callback_t;
    typedef CallbackBase<void,uint64_t,std::vector<std::vector<double> >,uint64_t,bool> Callback_v;
    class NVDIMM
    {
    public:
	void update(void);
	bool addTransaction(bool isWrite, uint64_t addr);
	void printStats(void);
	void saveStats(void);
	void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone, Callback_v *Power);
	void RegisterCallbacks(Callback_t *readDone, Callback_t *critLine, Callback_t *writeDone, Callback_v *Power); 

	void saveNVState(string filename);
	void loadNVState(string filename);
    };

    NVDIMM *getNVDIMMInstance(uint64_t id, string deviceFile, string sysFile, string pwd, string trc);
}

#endif
