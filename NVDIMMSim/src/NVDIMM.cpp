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


//NVDIMM.cpp
//Class file for nonvolatile memory dimm system wrapper

#include "NVDIMM.h"
#include "Init.h"

using namespace std;

//uint64_t BLOCKS_PER_PLANE;

namespace NVDSim
{
    NVDIMM::NVDIMM(uint64_t id, string deviceFile, string sysFile, string pwd, string trc) :
	dev(deviceFile),
	sys(sysFile),
	cDirectory(pwd)
    {
	uint64_t i, j;
	systemID = id;
	
	 if (cDirectory.length() > 0)
	 {
		 //ignore the pwd argument if the argument is an absolute path
		 if (dev[0] != '/')
		 {
		 dev = pwd + "/" + dev;
		 }
		 
		if (sys[0] != '/')
		 {
		 sys = pwd + "/" + sys;
		 }
	}
	Init::ReadIniFile(dev, false);
	//Init::ReadIniFile(sys, true);

	 if (!Init::CheckIfAllSet())
	 {
		 exit(-1);
	 }
	
	BLOCKS_PER_PLANE = (uint64_t)(VIRTUAL_BLOCKS_PER_PLANE * PBLOCKS_PER_VBLOCK);
	if(LOGGING == 1)
	{
	    PRINT("Logs are being generated");
	}else{
	    PRINT("Logs are not being generated");
	}
	PRINT("\nDevice Information:\n");
	PRINT("Device Type: "<<DEVICE_TYPE);
	uint64_t bits_per_GB = (uint64_t)(1024*8192)*(1024);
	PRINT("Size (GB): "<<TOTAL_SIZE/bits_per_GB);
	PRINT("Block Size: "<<BLOCK_SIZE);
	PRINT("Plane Size: "<<PLANE_SIZE);
	PRINT("Die Size: "<<DIE_SIZE);
	PRINT("Package Size: "<<PACKAGE_SIZE);
	PRINT("Total Size (KB): "<<TOTAL_SIZE);
	PRINT("Packages/Channels: "<<NUM_PACKAGES);
	PRINT("Page size: "<<NV_PAGE_SIZE);
	if(GARBAGE_COLLECT == 1)
	{
	  PRINT("Device is using garbage collection");
	}else{
	  PRINT("Device is not using garbage collection");
	}
	if(BUFFERED == 1 || FRONT_BUFFER == 1)
	{
	  PRINT("Memory is using a buffer between the channel and dies");
	}else{
	  PRINT("Memory channels are directly connected to dies");
	}
	PRINT("\nTiming Info:\n");
	PRINT("Read time: "<<READ_TIME);
	PRINT("Write Time: "<<WRITE_TIME);
	PRINT("Erase time: "<<ERASE_TIME);
	PRINT("Channel latency for data: "<<CHANNEL_CYCLE);
	PRINT("Channel width for data: "<<CHANNEL_WIDTH);
	PRINT("Device latency for data: "<<DEVICE_CYCLE);
	PRINT("Device width for data: "<<DEVICE_WIDTH)
	if(USE_EPOCHS == 1)
	{
	    PRINT("Device is using epoch data logging");
	}
	PRINT("Epoch Time: "<<EPOCH_CYCLES);
	PRINT("");
	
	
	if((GARBAGE_COLLECT == 0) && (DEVICE_TYPE.compare("NAND") == 0 || DEVICE_TYPE.compare("NOR") == 0))
	{
	  ERROR("Device is Flash and must use garbage collection");
	  exit(-1);
	}

	if(DEVICE_TYPE.compare("P8P") == 0)
	  {
	    if(ASYNC_READ_I == 0.0)
	      {
		WARNING("No asynchronous read current supplied, using 0.0");
	      }
	    else if(VPP_STANDBY_I == 0.0)
	       {
		PRINT("VPP standby current data missing for P8P, using 0.0");
	      }
	    else if(VPP_READ_I == 0.0)
	      {
		PRINT("VPP read current data missing for P8P, using 0.0");
	      }
	    else if(VPP_WRITE_I == 0.0)
	       {
		PRINT("VPP write current data missing for P8P, using 0.0");
	      }
	    else if(VPP_ERASE_I == 0.0)
	       {
		PRINT("VPP erase current data missing for P8P, using 0.0");
	      }
	    else if(VPP == 0.0)
	      {
		PRINT("VPP power data missing for P8P, using 0.0");
	      }
	  }
		
	if(DEVICE_TYPE.compare("P8P") == 0 && GARBAGE_COLLECT == 1)
	{
	    // if we're not logging we probably shouldn't even initialize the logger
	    if(LOGGING)
	    {
		log = new P8PGCLogger();
	    }
	    else
	    {
		log = NULL;
	    }
	    controller= new Controller(this, log);
	    ftl = new GCFtl(controller, log, this);
	}
	else if(DEVICE_TYPE.compare("P8P") == 0 && GARBAGE_COLLECT == 0)
	{
	    if(LOGGING)
	    {
		log = new P8PLogger();
	    }
	    else
	    {
		log = NULL;
	    }
	    controller= new Controller(this, log);
	    ftl = new Ftl(controller, log, this);
	}
	else if(GARBAGE_COLLECT == 1)
	{
	    if(LOGGING)
	    {
		log = new GCLogger();
	    }
	    else
	    {
		log = NULL;
	    }
	    controller= new Controller(this, log);
	    ftl = new GCFtl(controller, log, this);
	}
	else
	{
	    if(LOGGING)
	    {
		log = new Logger();
	    }
	    else
	    {
		log = NULL;
	    }
	    controller= new Controller(this, log);
	    ftl = new Ftl(controller, log, this);
	}
	packages= new vector<Package>();

	if (DIES_PER_PACKAGE > INT_MAX){
		ERROR("Too many dies.");
		exit(1);
	}

	// sanity checks
	

	for (i= 0; i < NUM_PACKAGES; i++){
	    Package pack = {new Channel(), new Buffer(i), vector<Die *>()};
	    //pack.channel= new Channel();
	    pack.channel->attachController(controller);
	    pack.channel->attachBuffer(pack.buffer);
	    pack.buffer->attachChannel(pack.channel);
	    for (j= 0; j < DIES_PER_PACKAGE; j++){
		Die *die= new Die(this, log, j);
		die->attachToBuffer(pack.buffer);
		pack.buffer->attachDie(die);
		pack.dies.push_back(die);
	    }
	    packages->push_back(pack);
	}
	controller->attachPackages(packages);

	frontBuffer = new FrontBuffer(this, ftl);
	controller->attachFrontBuffer(frontBuffer);

	ReturnReadData= NULL;
	WriteDataDone= NULL;

	epoch_count = 0;
	epoch_cycles = 0;

	numReads= 0;
	numWrites= 0;
	numErases= 0;
	currentClockCycle= 0;
	cycles_left = new uint64_t [NUM_PACKAGES];
	for(uint64_t h = 0; h < NUM_PACKAGES; h++){
	    cycles_left[h] = 0;
	}
	// the channel and buffers are running faster than the other parts of the system
	/*if(CYCLE_TIME > CHANNEL_CYCLE)
	{
	    channel_cycles_per_cycle = (uint64_t)(((float)CYCLE_TIME / (float)CHANNEL_CYCLE) + 0.50f);
	    faster_channel = true;
	}
	else if(CYCLE_TIME <= CHANNEL_CYCLE)
	{
	    channel_cycles_per_cycle = (uint64_t)(((float)CHANNEL_CYCLE / (float)CYCLE_TIME) + 0.50f);
	    faster_channel = false;
	}
	cout << "the faster cycles computed was: " << channel_cycles_per_cycle << " \n";*/

	// counters for cross clock domain calculations
	system_clock_counter = 0.0;
	nv_clock_counter1 = 0.0;
	nv_clock_counter2 = 0.0;
	controller_clock_counter = 0.0;
	nv_clock_counter3 = new float [NUM_PACKAGES];
	channel_clock_counter = new float [NUM_PACKAGES];
	for(uint64_t c = 0; c < NUM_PACKAGES; c++){
	    nv_clock_counter3[c] = 0.0;
	    channel_clock_counter[c] = 0.0;
	}
	
	ftl->loadNVState();
    }

// static allocator for the library interface
    NVDIMM *getNVDIMMInstance(uint64_t id, string deviceFile, string sysFile, string pwd, string trc)
    {
	return new NVDIMM(id, deviceFile, sysFile, pwd, trc);
    }

    bool NVDIMM::add(FlashTransaction &trans){
	if(FRONT_BUFFER)
	{
	    return frontBuffer->addTransaction(trans);
	}
	else
	{
	    return ftl->addTransaction(trans);
	}
    }

    bool NVDIMM::addTransaction(bool isWrite, uint64_t addr){
	TransactionType type = isWrite ? DATA_WRITE : DATA_READ;
	FlashTransaction trans = FlashTransaction(type, addr, NULL);
	if(FRONT_BUFFER)
	{
	    return frontBuffer->addTransaction(trans);
	}
	else
	{
	    return ftl->addTransaction(trans);
	}
    }

    string NVDIMM::SetOutputFileName(string tracefilename){
	return "";
    }

    void NVDIMM::RegisterCallbacks(Callback_t *readCB, Callback_t *writeCB, Callback_v *Power){
	ReturnReadData = readCB;
	CriticalLineDone = NULL;
	WriteDataDone = writeCB;
	ReturnPowerData = Power;
    }

    void NVDIMM::RegisterCallbacks(Callback_t *readCB,  Callback_t *critLineCB, Callback_t *writeCB, Callback_v *Power)
    {
	ReturnReadData = readCB;
	CriticalLineDone = critLineCB;
	WriteDataDone = writeCB;
	ReturnPowerData = Power;
    }

    void NVDIMM::printStats(void){
	if(LOGGING == true)
	{
	    log->print(currentClockCycle);
	}
    }

    void NVDIMM::saveStats(void){
	if(LOGGING == true)
	{
	    log->save(currentClockCycle, epoch_count);
	}
	ftl->saveNVState();
    }

    void NVDIMM::update(void)
    {
	uint64_t i, j;
	Package package;

	//update the system clock counters
	system_clock_counter += SYSTEM_CYCLE;

	while(nv_clock_counter1 < system_clock_counter)
	{
	    nv_clock_counter1 += CYCLE_TIME;

	    //cout << "updating ftl \n";
	    ftl->update();
	    ftl->step();
	    
	    if(BUFFERED)
	    {
		nv_clock_counter2 += CYCLE_TIME;
		while(controller_clock_counter < nv_clock_counter2)
		{
		    controller_clock_counter += CHANNEL_CYCLE;
		    controller->update();
		    controller->step();
		}

		if(controller_clock_counter == nv_clock_counter2)
		{
		    nv_clock_counter2 = 0.0;
		    controller_clock_counter = 0.0;
		}
		/*
		if(faster_channel)
		{
		    for(uint64_t c = 0; c < channel_cycles_per_cycle; c++)
		    {
			controller->update();
			controller->step();
		    }
		}
		else
		{
		    // reset the update counter and update the controller
		    if(controller_cycles_left == 0)
		    {
			controller->update();
			controller->step();
			controller_cycles_left = channel_cycles_per_cycle;
		    }
		    
		    controller_cycles_left = controller_cycles_left - 1;
		}
		*/
	    }
	    else if(FRONT_BUFFER)
	    {
		nv_clock_counter2 += CYCLE_TIME;
		while(controller_clock_counter < nv_clock_counter2)
		{
		    controller_clock_counter += CHANNEL_CYCLE;
		    frontBuffer->update();
		    frontBuffer->step();
		}

		if(controller_clock_counter == nv_clock_counter2)
		{
		    nv_clock_counter2 = 0.0;
		    controller_clock_counter = 0.0;
		}

		controller->update();
		controller->step();
	    }
	    else
	    {
		controller->update();
		controller->step();
	    }
	
	    for (i= 0; i < packages->size(); i++){
		package= (*packages)[i];
		if(BUFFERED)
		{
		    nv_clock_counter3[i] += CYCLE_TIME;
		    while(channel_clock_counter[i] < nv_clock_counter3[i])
		    {
			channel_clock_counter[i] += CHANNEL_CYCLE;
			package.channel->update();
			package.buffer->update();
		    }

		    if(channel_clock_counter[i] == nv_clock_counter3[i])
		    {
			channel_clock_counter[i] = 0.0;
			nv_clock_counter3[i] = 0.0;
		    }
		    /*if(faster_channel)
		    {
			for(uint64_t c = 0; c < channel_cycles_per_cycle; c++)
			{
			    package.channel->update();
			    package.buffer->update();
			}
		    }
		    else
		    {
			// reset the update counter and update the channel
			if(cycles_left[i] == 0)
			{
			    package.channel->update();
			    package.buffer->update();
			    cycles_left[i] = channel_cycles_per_cycle;
			}
			
			cycles_left[i] = cycles_left[i] - 1;
		    }*/
		}
		else
		{
		    package.channel->update();
		    package.buffer->update();
		}		
		for (j= 0; j < package.dies.size() ; j++)
		{
			package.dies[j]->update();
			package.dies[j]->step();
		}
	    }

	    if(LOGGING == true)
	    {
		log->update();
	    }
	    
	    step();
	    
	    //saving stats at the end of each epoch
	    if(USE_EPOCHS)
	    {
		ftl->sendQueueLength();
		controller->sendQueueLength();
		if(epoch_cycles >= EPOCH_CYCLES)
		{
		    if(LOGGING == true)
		    {
			log->save_epoch(currentClockCycle, epoch_count);
			log->ftlQueueReset();
			log->ctrlQueueReset();
		    }
		    epoch_count++;
		    epoch_cycles = 0;		
		}
		else
		{
		    epoch_cycles++;
		}
	    }
	}

	if(nv_clock_counter1 == system_clock_counter)
	{
	    nv_clock_counter1 = 0.0;
	    system_clock_counter = 0.0;
	}

	//cout << "NVDIMM successfully updated" << endl;
    }

    void NVDIMM::powerCallback(void){
	ftl->powerCallback();
    }

//If either of these methods are called it is because HybridSim called them
//therefore the appropriate system setting should be set
    void NVDIMM::saveNVState(string filename){
	ENABLE_NV_SAVE = 1;
	NV_SAVE_FILE = filename;
	cout << "got to save state in nvdimm \n";
	cout << "save file was " << NV_SAVE_FILE << "\n";
	ftl->saveNVState();
    }

    void NVDIMM::loadNVState(string filename){
	ENABLE_NV_RESTORE = 1;
	NV_RESTORE_FILE = filename;
	ftl->loadNVState();
    }

    void NVDIMM::queuesNotFull(void)
    {
	ftl->queuesNotFull();
    }

    void NVDIMM::GCReadDone(uint64_t vAddr)
    {
	ftl->GCReadDone(vAddr);
    }
}
