/******************************
 * created on 2015/5/6
 ******************************/
#include "NVM/nvmain.h"
#include "NVM/RBLA_NVMain/RBLA_NVMain.h"
#include "src/NVMObject.h"
#include "string.h"


namespace NVM
{
	class NVMainFactory
	{
		public:
			static NVMObject* CreateNewNVMain( std::string nvmain_type );
		private:
			NVMain* CreateNVMain( std::string nvmain_type);
	};
};
