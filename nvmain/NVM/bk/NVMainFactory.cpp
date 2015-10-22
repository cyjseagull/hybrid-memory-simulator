/*********************
 * created on 2015.5.6
 ********************/

#include "NVMainFactory.h"
#include "include/Exception.h"

NVMain *NVMainFactory::CreateNVMain( std::string nvm_type) 
{
	NVMObject* main_mem = NULL;
	if( nvm_type == "NVMain") main_mem = new NVMain();
	if( nvm_type == "RBLA_NVMain") main_mem = new RBLA_NVMain();
	return main_mem;
}

NVMObject *NVMainFactory::CreateNewNVMain( std::string nvm_type)
{
	NVMObject* main_mem = CreateNVMain(nvm_type);
	if( !main_mem )
	{
		NVM::Warning("didn't set main memory type! default set NVMain object as main memory");
		main_mem = CreateNVMain(nvm_type);
	}
	return main_mem;
}
