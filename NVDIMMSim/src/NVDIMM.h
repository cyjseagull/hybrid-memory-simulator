/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

#ifndef NVDIMM_H
#define NVDIMM_H
//NVDIMM.h
//Header for nonvolatile memory dimm system wrapper

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Controller.h"
#include "Ftl.h"
#include "GCFtl.h"
#include "Die.h"
#include "FlashTransaction.h"
#include "Callbacks.h"
#include "Logger.h"
#include "GCLogger.h"
#include "P8PLogger.h"
#include "P8PGCLogger.h"
#include "FrontBuffer.h"
#include "Util.h"

using std::string;

namespace NVDSim{
    typedef CallbackBase<void,uint64_t,uint64_t,uint64_t,bool> Callback_t;
    typedef CallbackBase<void,uint64_t,vector<vector<double>>,uint64_t,bool> Callback_v;
	class NVDIMM : public SimObj{
		public:
			NVDIMM(uint64_t id, string dev, string sys, string pwd, string trc);
			void update(void);
			bool add(FlashTransaction &trans);
			bool addTransaction(bool isWrite, uint64_t addr);
			void printStats(void);
			void saveStats(void);
			string SetOutputFileName(string tracefilename);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone, Callback_v *Power);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *critLine, Callback_t *writeDone, Callback_v *Power); 

			void powerCallback(void);

			void saveNVState(string filename);
			void loadNVState(string filename);

			void queuesNotFull(void);

			void GCReadDone(uint64_t vAddr);

			Controller *controller;
			Ftl *ftl;
			Logger *log;
			FrontBuffer  *frontBuffer;

			vector<Package> *packages;

			Callback_t* ReturnReadData;
			Callback_t* CriticalLineDone;
			Callback_t* WriteDataDone;
			Callback_v* ReturnPowerData;

			uint64_t systemID, numReads, numWrites, numErases;
			uint64_t epoch_count, epoch_cycles;
			uint64_t channel_cycles_per_cycle, controller_cycles_left;
			float system_clock_counter, nv_clock_counter1, nv_clock_counter2, controller_clock_counter;
			float *channel_clock_counter, *nv_clock_counter3;
			uint64_t* cycles_left;
	
			bool faster_channel;

		private:
			string dev, sys, cDirectory;
	};

	NVDIMM *getNVDIMMInstance(uint64_t id, string deviceFile, string sysFile, string pwd, string trc);
}
#endif
