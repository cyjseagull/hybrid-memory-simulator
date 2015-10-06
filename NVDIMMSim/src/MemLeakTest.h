#ifndef NVMLT
#define NVMLT

#include NVDIMM.h

class test_obj{
	public:
		void read_cb(uint, uint64_t, uint64_t);
		void write_cb(uint, uint64_t, uint64_t);
		void power_cb(uint, vector<vector<double>>, uint64_t);
		void run_test(void);
};
#endif
