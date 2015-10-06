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

#include "Init.h"

using namespace std;

// these are the values that are extern'd in FlashConfiguration.h so that they
// have global scope even though they are set by Init

namespace NVDSim 
{

    bool SCHEDULE;
    bool WRITE_ON_QUEUE_SIZE;
    uint64_t WRITE_QUEUE_LIMIT;
    bool IDLE_WRITE;
    bool CTRL_SCHEDULE;
    bool CTRL_WRITE_ON_QUEUE_SIZE;
    uint64_t CTRL_WRITE_QUEUE_LIMIT;
    bool CTRL_IDLE_WRITE;
    bool PERFECT_SCHEDULE;
    bool ENABLE_WRITE_SCRIPT;
    std::string NV_WRITE_SCRIPT;
    bool DELAY_WRITE;
    uint64_t DELAY_WRITE_CYCLES;
    bool DISK_READ;

    bool FRONT_BUFFER;
    uint64_t REQUEST_BUFFER_SIZE;
    uint64_t RESPONSE_BUFFER_SIZE;
    bool BUFFERED;
    bool CUT_THROUGH;
    uint64_t IN_BUFFER_SIZE;
    uint64_t OUT_BUFFER_SIZE;
    
    bool CRIT_LINE_FIRST;
    
    bool LOGGING;
    std::string LOG_DIR;
    bool WEAR_LEVEL_LOG;
    bool RUNTIME_WRITE; 
    bool PER_PACKAGE;
    bool QUEUE_EVENT_LOG;
    bool PLANE_STATE_LOG;
    bool WRITE_ARRIVE_LOG;
    bool READ_ARRIVE_LOG;

    bool ENABLE_NV_SAVE;
    std::string NV_SAVE_FILE;
    bool ENABLE_NV_RESTORE;
    std::string NV_RESTORE_FILE;

    std::string DEVICE_TYPE;
    uint64_t NUM_PACKAGES;
    uint64_t DIES_PER_PACKAGE;
    uint64_t PLANES_PER_DIE;
    uint64_t BLOCKS_PER_PLANE;
    uint64_t VIRTUAL_BLOCKS_PER_PLANE;
    uint64_t PAGES_PER_BLOCK;
    uint64_t NV_PAGE_SIZE;
    float DEVICE_CYCLE;
    uint64_t DEVICE_WIDTH;

    float CHANNEL_CYCLE; //default channel, becomes up channel when down channel is enabled
    uint64_t CHANNEL_WIDTH;

    bool ENABLE_COMMAND_CHANNEL;
    uint64_t COMMAND_CHANNEL_WIDTH;

    bool ENABLE_REQUEST_CHANNEL;
    uint64_t REQUEST_CHANNEL_WIDTH;

    bool GARBAGE_COLLECT;
    bool PRESTATE;
    float PERCENT_FULL;
    
    uint64_t READ_TIME;
    uint64_t WRITE_TIME;
    uint64_t ERASE_TIME;
    uint64_t COMMAND_LENGTH;
    uint64_t LOOKUP_TIME;
    uint64_t BUFFER_LOOKUP_TIME;
    uint64_t QUEUE_ACCESS_TIME;
    uint64_t EPOCH_CYCLES;
    float CYCLE_TIME;
    float SYSTEM_CYCLE;

    uint64_t FTL_READ_QUEUE_LENGTH;
    uint64_t CTRL_READ_QUEUE_LENGTH;
    uint64_t FTL_WRITE_QUEUE_LENGTH;
    uint64_t CTRL_WRITE_QUEUE_LENGTH;

    double READ_I;
    double WRITE_I;
    double ERASE_I;
    double STANDBY_I;
    double IN_LEAK_I;
    double OUT_LEAK_I;
    double VCC;
    double ASYNC_READ_I;
    double VPP_STANDBY_I;
    double VPP_READ_I;
    double VPP_WRITE_I;
    double VPP_ERASE_I;
    double VPP;

    float IDLE_GC_THRESHOLD;
    float FORCE_GC_THRESHOLD;
    float PBLOCKS_PER_VBLOCK;
    
    bool DEBUG_INIT= 0;
		
    //Map the string names to the variables they set
    static ConfigMap configMap[] = {
	//DEFINE_UINT_PARAM -- see Init.h
	DEFINE_BOOL_PARAM(SCHEDULE, DEV_PARAM),
	DEFINE_BOOL_PARAM(WRITE_ON_QUEUE_SIZE, DEV_PARAM),
	DEFINE_UINT64_PARAM(WRITE_QUEUE_LIMIT, DEV_PARAM),
	DEFINE_BOOL_PARAM(IDLE_WRITE, DEV_PARAM),
	DEFINE_BOOL_PARAM(CTRL_SCHEDULE, DEV_PARAM),
	DEFINE_BOOL_PARAM(CTRL_WRITE_ON_QUEUE_SIZE, DEV_PARAM),
	DEFINE_UINT64_PARAM(CTRL_WRITE_QUEUE_LIMIT, DEV_PARAM),
	DEFINE_BOOL_PARAM(CTRL_IDLE_WRITE, DEV_PARAM),
	DEFINE_BOOL_PARAM(PERFECT_SCHEDULE, DEV_PARAM),
	DEFINE_BOOL_PARAM(ENABLE_WRITE_SCRIPT, DEV_PARAM),
	DEFINE_STRING_PARAM(NV_WRITE_SCRIPT, DEV_PARAM),
	DEFINE_BOOL_PARAM(DELAY_WRITE, DEV_PARAM),
	DEFINE_UINT64_PARAM(DELAY_WRITE_CYCLES, DEV_PARAM),
	DEFINE_BOOL_PARAM(DISK_READ, DEV_PARAM),
	DEFINE_BOOL_PARAM(FRONT_BUFFER, DEV_PARAM),
	DEFINE_UINT64_PARAM(REQUEST_BUFFER_SIZE, DEV_PARAM),
	DEFINE_UINT64_PARAM(RESPONSE_BUFFER_SIZE, DEV_PARAM),
	DEFINE_BOOL_PARAM(BUFFERED, DEV_PARAM),
	DEFINE_BOOL_PARAM(CUT_THROUGH, DEV_PARAM),
	DEFINE_UINT64_PARAM(IN_BUFFER_SIZE, DEV_PARAM),
	DEFINE_UINT64_PARAM(OUT_BUFFER_SIZE, DEV_PARAM),
	DEFINE_BOOL_PARAM(CRIT_LINE_FIRST, DEV_PARAM),
	DEFINE_BOOL_PARAM(LOGGING, DEV_PARAM),
	DEFINE_STRING_PARAM(LOG_DIR, DEV_PARAM),
	DEFINE_BOOL_PARAM(WEAR_LEVEL_LOG, DEV_PARAM),
	DEFINE_BOOL_PARAM(RUNTIME_WRITE, DEV_PARAM),
	DEFINE_BOOL_PARAM(PER_PACKAGE, DEV_PARAM),
	DEFINE_BOOL_PARAM(QUEUE_EVENT_LOG, DEV_PARAM),
	DEFINE_BOOL_PARAM(PLANE_STATE_LOG, DEV_PARAM),
	DEFINE_BOOL_PARAM(WRITE_ARRIVE_LOG, DEV_PARAM),
	DEFINE_BOOL_PARAM(READ_ARRIVE_LOG, DEV_PARAM),
	DEFINE_BOOL_PARAM(ENABLE_NV_SAVE, DEV_PARAM),
	DEFINE_STRING_PARAM(NV_SAVE_FILE, DEV_PARAM),
	DEFINE_BOOL_PARAM(ENABLE_NV_RESTORE, DEV_PARAM),
	DEFINE_STRING_PARAM(NV_RESTORE_FILE, DEV_PARAM),
	DEFINE_STRING_PARAM(DEVICE_TYPE, DEV_PARAM),
	DEFINE_UINT64_PARAM(NUM_PACKAGES,DEV_PARAM),
	DEFINE_UINT64_PARAM(DIES_PER_PACKAGE,DEV_PARAM),
	DEFINE_UINT64_PARAM(PLANES_PER_DIE,DEV_PARAM),
	//DEFINE_UINT64_PARAM(BLOCKS_PER_PLANE,DEV_PARAM),
	DEFINE_UINT64_PARAM(VIRTUAL_BLOCKS_PER_PLANE,DEV_PARAM),
	DEFINE_UINT64_PARAM(PAGES_PER_BLOCK,DEV_PARAM),
	DEFINE_UINT64_PARAM(NV_PAGE_SIZE,DEV_PARAM),
	DEFINE_FLOAT_PARAM(DEVICE_CYCLE,DEV_PARAM),
	DEFINE_UINT64_PARAM(DEVICE_WIDTH,DEV_PARAM),
	DEFINE_FLOAT_PARAM(CHANNEL_CYCLE,DEV_PARAM),
	DEFINE_UINT64_PARAM(CHANNEL_WIDTH,DEV_PARAM),
	DEFINE_BOOL_PARAM(ENABLE_COMMAND_CHANNEL,DEV_PARAM),
	DEFINE_UINT64_PARAM(COMMAND_CHANNEL_WIDTH,DEV_PARAM),
        DEFINE_BOOL_PARAM(ENABLE_REQUEST_CHANNEL,DEV_PARAM),
	DEFINE_UINT64_PARAM(REQUEST_CHANNEL_WIDTH,DEV_PARAM),
	DEFINE_BOOL_PARAM(GARBAGE_COLLECT,DEV_PARAM),
	DEFINE_BOOL_PARAM(PRESTATE,DEV_PARAM),
	DEFINE_FLOAT_PARAM(PERCENT_FULL,DEV_PARAM),
	DEFINE_UINT64_PARAM(READ_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(WRITE_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(ERASE_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(COMMAND_LENGTH,DEV_PARAM),
	DEFINE_UINT64_PARAM(LOOKUP_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(BUFFER_LOOKUP_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(QUEUE_ACCESS_TIME,DEV_PARAM),
	DEFINE_UINT64_PARAM(EPOCH_CYCLES,DEV_PARAM),
	DEFINE_FLOAT_PARAM(CYCLE_TIME,DEV_PARAM),
	DEFINE_FLOAT_PARAM(SYSTEM_CYCLE,DEV_PARAM),
	DEFINE_UINT64_PARAM(FTL_READ_QUEUE_LENGTH,DEV_PARAM),
	DEFINE_UINT64_PARAM(CTRL_READ_QUEUE_LENGTH,DEV_PARAM),
	DEFINE_UINT64_PARAM(FTL_WRITE_QUEUE_LENGTH,DEV_PARAM),
	DEFINE_UINT64_PARAM(CTRL_WRITE_QUEUE_LENGTH,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(READ_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(WRITE_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(ERASE_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(STANDBY_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(IN_LEAK_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(OUT_LEAK_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VCC,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(ASYNC_READ_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VPP_STANDBY_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VPP_READ_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VPP_WRITE_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VPP_ERASE_I,DEV_PARAM),
	DEFINE_DOUBLE_PARAM(VPP,DEV_PARAM),
	DEFINE_FLOAT_PARAM(IDLE_GC_THRESHOLD,DEV_PARAM),
	DEFINE_FLOAT_PARAM(FORCE_GC_THRESHOLD,DEV_PARAM),
	DEFINE_FLOAT_PARAM(PBLOCKS_PER_VBLOCK,DEV_PARAM),
	
	{"", NULL, UINT64, SYS_PARAM, false} // tracer value to signify end of list; if you delete it, epic fail will result
    };

    void Init::WriteValuesOut(std::ofstream &visDataOut) 
    {
	//DEBUG("WRITE CALLED");
	visDataOut<<"!!SYSTEM_INI"<<endl;
	for (size_t i=0; configMap[i].variablePtr != NULL; i++) 
	{
	    if (configMap[i].parameterType == SYS_PARAM) 
	    {
		visDataOut<<configMap[i].iniKey<<"=";
		switch (configMap[i].variableType) 
		{
		    //parse and set each type of variable
		case UINT:
		    visDataOut << *((uint *)configMap[i].variablePtr);
		    break;
		case UINT64:
		    visDataOut << *((uint64_t *)configMap[i].variablePtr);
		    break;
		case FLOAT:
		    visDataOut << *((float *)configMap[i].variablePtr);
		    break;
		case DOUBLE:
		    visDataOut << *((double *)configMap[i].variablePtr);
		    break;
		case STRING:
		    visDataOut << *((string *)configMap[i].variablePtr);
		    break;
		case BOOL:
		    if (*((bool *)configMap[i].variablePtr)) {
			visDataOut <<"true";
		    } else {
			visDataOut <<"false";
		    }
		    break;
		}
		visDataOut << endl;
	    }
	}
	
    }
    
    void Init::SetKey(string key, string valueString, bool isSystemParam, size_t lineNumber) 
    {
	size_t i;
	uint intValue;
	uint64_t int64Value;
	float floatValue;
	double doubleValue;
	
	for (i=0; configMap[i].variablePtr != NULL; i++) 
	{	
	    istringstream iss(valueString);
	    // match up the string in the config map with the key we parsed
	    if (key.compare(configMap[i].iniKey) == 0) {
		switch (configMap[i].variableType) {
		    //parse and set each type of variable
		case UINT:
		    if ((iss >> dec >> intValue).fail()) 
		    {
			ERROR("could not parse line "<<lineNumber<<" (non-numeric value '"<<valueString<<"')?");
		    }
		    *((uint *)configMap[i].variablePtr) = intValue;
		    if (DEBUG_INIT)
		    {
			DEBUG("\t - SETTING "<<configMap[i].iniKey<<"="<<intValue);
		    }
		    break;
		case UINT64:
		    if ((iss >> dec >> int64Value).fail()) 
		    {
			ERROR("could not parse line "<<lineNumber<<" (non-numeric value '"<<valueString<<"')?");
		    }
		    *((uint64_t *)configMap[i].variablePtr) = int64Value;
		    if (DEBUG_INIT)
		    {
			DEBUG("\t - SETTING "<<configMap[i].iniKey<<"="<<int64Value);
		    }
		    break;
		case FLOAT:
		    if ((iss >> dec >> floatValue).fail()) 
		    {
			ERROR("could not parse line "<<lineNumber<<" (non-numeric value '"<<valueString<<"')?");
		    }
		    *((float *)configMap[i].variablePtr) = floatValue;
		    if (DEBUG_INIT)
		    {
			DEBUG("\t - SETTING "<<configMap[i].iniKey<<"="<<floatValue);
		    }
		    break;
		case DOUBLE:
		    if ((iss >> dec >> doubleValue).fail()) 
		    {
			ERROR("could not parse line "<<lineNumber<<" (non-numeric value '"<<valueString<<"')?");
		    }
		    *((double *)configMap[i].variablePtr) = doubleValue;
		    if (DEBUG_INIT)
		    {
			DEBUG("\t - SETTING "<<configMap[i].iniKey<<"="<<doubleValue);
		    }
		    break;
		case STRING:
		    *((string *)configMap[i].variablePtr) = string(valueString);
		    if (DEBUG_INIT)
		    {
			DEBUG("\t - SETTING "<<configMap[i].iniKey<<"="<<valueString);
		    }
		    
		    break;
		case BOOL:
		    if (valueString == "true" || valueString == "1") {
			*((bool *)configMap[i].variablePtr) = true;
		    } else {
			*((bool *)configMap[i].variablePtr) = false;
		    }
		}
		// lineNumber == 0 implies that this is an override parameter from the command line, so don't bother doing these checks
		if (lineNumber > 0) 
		{
		    if (isSystemParam && configMap[i].parameterType == DEV_PARAM) 
		    {
			DEBUG("WARNING: Found device parameter "<<configMap[i].iniKey<<" in system config file");
		    } 
		    else if (!isSystemParam && configMap[i].parameterType == SYS_PARAM) 
		    {
			DEBUG("WARNING: Found system parameter "<<configMap[i].iniKey<<" in device config file");
		    }
		}
		// use the pointer stored in the config map to set the value of the variable
		// to make sure all parameters are in the ini file
		configMap[i].wasSet = true;
		break;
	    }
	}

	if (configMap[i].variablePtr == NULL) 
	{
	    DEBUG("WARNING: UNKNOWN KEY '"<<key<<"' IN INI FILE");
	}
    }
    
    void Init::ReadIniFile(string filename, bool isSystemFile)
    {
	ifstream iniFile;
	string line;
	string key,valueString;
	
	size_t commentIndex, equalsIndex;
	size_t lineNumber=0;

	iniFile.open(filename.c_str());
	if (iniFile.is_open())
	{
	    while (!iniFile.eof()) 
	    {
		lineNumber++;
		//cout<<line<<endl;
		getline(iniFile, line);
		//this can happen if the filename is actually a directory
		if (iniFile.bad()) 
		{
		    ERROR("Cannot read ini file '"<<filename<<"'");
		    exit(-1);
		}
		// skip zero-length lines
		if (line.size() == 0)
		{
//		DEBUG("Skipping blank line "<<lineNumber);
		    continue;
		}
		//search for a comment char
		if ((commentIndex = line.find_first_of(";")) != string::npos) 
		{
		    //if the comment char is the first char, ignore the whole line
		    if (commentIndex == 0) 
		    {
//		    DEBUG("Skipping comment line "<<lineNumber);
			continue;
		    }
//		DEBUG("Truncating line at comment"<<line[commentIndex-1]);
		    //truncate the line at first comment before going on
		    line = line.substr(0,commentIndex); 
		}
		// trim off the end spaces that might have been between the value and comment char
		size_t whiteSpaceEndIndex;
		if ((whiteSpaceEndIndex = line.find_last_not_of(" \t")) != string::npos)
		{
		    line = line.substr(0,whiteSpaceEndIndex+1);
		}
		
		// at this point line should be a valid, commentless string
		
		// a line has to have an equals sign
		if ((equalsIndex = line.find_first_of("=")) == string::npos)
		{
		    ERROR("Malformed Line "<<lineNumber<<" (missing equals)");
		    abort();
		}
		size_t strlen = line.size();
		// all characters before the equals are the key
		key = line.substr(0, equalsIndex);
		// all characters after the equals are the value
		valueString = line.substr(equalsIndex+1,strlen-equalsIndex);
		
		Init::SetKey(key, valueString, lineNumber, isSystemFile);
		// got to the end of the config map without finding the key
	    }
	}
	else
	{
	    ERROR ("Unable to load ini file "<<filename);
	    abort();
	}
    }
    
    void Init::OverrideKeys(vector<string> keys, vector<string>values) 
    {
	if (keys.size() != values.size()) {
	    ERROR("-o option is messed up");
	    exit(-1);
	}
	for (size_t i=0; i<keys.size(); i++) {
	    Init::SetKey(keys[i], values[i]);
	}
    }
    
    bool Init::CheckIfAllSet() {
	// check to make sure all parameters that we exepected were set 
	for (size_t i=0; configMap[i].variablePtr != NULL; i++) 
	{
	    if (!configMap[i].wasSet) 
	    {
		DEBUG("WARNING: KEY "<<configMap[i].iniKey<<" NOT FOUND IN INI FILE.");
		switch (configMap[i].variableType) 
		{
		    //the string and bool values can be defaulted, but generally we need all the numeric values to be set to continue
		case UINT: 
		    if (configMap[i].iniKey.compare((std::string)"FTL_QUEUE_LENGTH") == 0 ||
			configMap[i].iniKey.compare((std::string)"CTRL_QUEUE_LENGTH") == 0 ||
			configMap[i].iniKey.compare((std::string)"WRITE_QUEUE_LIMIT") == 0)
		    {
			*((uint *)configMap[i].variablePtr) = 0;
			DEBUG("\tSetting Default: "<<configMap[i].iniKey<<"=0");
			break;
		    }
		case UINT64:
		case FLOAT:
		    ERROR("Cannot continue without key '"<<configMap[i].iniKey<<"' set.");
		    return false;
		    break;
		case DOUBLE:
		    if (configMap[i].iniKey.compare((std::string)"ASYNC_READ_I") == 0 ||
			configMap[i].iniKey.compare((std::string)"VPP_STANDBY_I") == 0 ||
			configMap[i].iniKey.compare((std::string)"VPP_READ_I") == 0 ||
			configMap[i].iniKey.compare((std::string)"VPP_WRITE_I") == 0 ||
			configMap[i].iniKey.compare((std::string)"VPP_ERASE_I") == 0 ||
			configMap[i].iniKey.compare((std::string)"VPP") == 0)
		    {
			*((double *)configMap[i].variablePtr) = 0.0;
			DEBUG("\tSetting Default: "<<configMap[i].iniKey<<"=0.0");
		    }		  
		    else
		    {
			ERROR("Cannot continue without key '"<<configMap[i].iniKey<<"' set.");
			return false;
		    }
		    break;
		case BOOL:
		    *((bool *)configMap[i].variablePtr) = false;
		    DEBUG("\tSetting Default: "<<configMap[i].iniKey<<"=false");
		    break;
		case STRING:
		    break;
		}
	    }
	}
	return true;
    }
    /*unecessary right now
      void Init::InitEnumsFromStrings() {
      if (ADDRESS_MAPPING_SCHEME == "scheme1") {
      addressMappingScheme = Scheme1;
      DEBUG("ADDR SCHEME: 1");
      } else if (ADDRESS_MAPPING_SCHEME == "scheme2") {
      addressMappingScheme = Scheme2;
      DEBUG("ADDR SCHEME: 2");
      } else if (ADDRESS_MAPPING_SCHEME == "scheme3") {
      addressMappingScheme = Scheme3;
      DEBUG("ADDR SCHEME: 3");
      } else if (ADDRESS_MAPPING_SCHEME == "scheme4") {
      addressMappingScheme = Scheme4;
      DEBUG("ADDR SCHEME: 4");
      } else if (ADDRESS_MAPPING_SCHEME == "scheme5") {
      addressMappingScheme = Scheme5;
      DEBUG("ADDR SCHEME: 5");
      } else if (ADDRESS_MAPPING_SCHEME == "scheme6") {
      addressMappingScheme = Scheme6;
      DEBUG("ADDR SCHEME: 6");
      } else {
      cout << "WARNING: unknown address mapping scheme '"<<ADDRESS_MAPPING_SCHEME<<"'; valid values are 'scheme1', 'scheme2', 'scheme3', 'scheme4', 'scheme5'. Defaulting to scheme1"<<endl;
      addressMappingScheme = Scheme1;
      }
      
      if (ROW_BUFFER_POLICY == "open_page") {
      rowBufferPolicy = OpenPage;
      DEBUG("ROW BUFFER: open page");
      } else if (ROW_BUFFER_POLICY == "close_page") {
      rowBufferPolicy = ClosePage;
      DEBUG("ROW BUFFER: close page");
      } else {
      cout << "WARNING: unknown row buffer policy '"<<ROW_BUFFER_POLICY<<"'; valid values are 'open_page' or 'close_page', Defaulting to Close Page."<<endl;
      rowBufferPolicy = ClosePage;
      }
      
      if (QUEUING_STRUCTURE == "per_rank_per_bank") {
      queuingStructure = PerRankPerBank;
      DEBUG("QUEUING STRUCT: per rank per bank");
      } else if (QUEUING_STRUCTURE == "per_rank") {
      queuingStructure = PerRank;
      DEBUG("QUEUING STRUCT: per rank");
      } else {
      cout << "WARNING: Unknown queueing structure '"<<QUEUING_STRUCTURE<<"'; valid options are 'per_rank' and 'per_rank_per_bank', defaulting to Per Rank Per Bank"<<endl;
      queuingStructure = PerRankPerBank;
      }

      if (SCHEDULING_POLICY == "rank_then_bank_round_robin") {
      schedulingPolicy = RankThenBankRoundRobin;
      DEBUG("SCHEDULING: Rank Then Bank");
      } else if (SCHEDULING_POLICY == "bank_then_rank_round_robin") {
      schedulingPolicy = BankThenRankRoundRobin;
      DEBUG("SCHEDULING: Bank Then Rank");
      } else {
      cout << "WARNING: Unknown scheduling policy '"<<SCHEDULING_POLICY<<"'; valid options are 'rank_then_bank_round_robin' or 'bank_then_rank_round_robin'; defaulting to Bank Then Rank Round Robin" << endl;
      schedulingPolicy = BankThenRankRoundRobin;
      }
      
      }*/

#if 0
    // Wrote it, but did not use it -- might be handy in the future
    void Init::Trim(string &str) 
    {
	size_t begin,end;
	if ((begin = str.find_first_not_of(" ")) == string::npos) {
	    begin = 0;
	}
	if ((end = str.find_last_not_of(" ")) == string::npos) 
	{
	    end = str.size()-1;
	}
	str = str.substr(begin,end-begin+1);
    }
#endif

}
