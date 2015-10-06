#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <stdint.h>

using namespace std;

//typedef unsigned long long int uint64_t;

const uint64_t OP_READ = 0;
const uint64_t OP_WRITE = 1;

const uint64_t PAGE_SIZE = 1024;
const uint64_t SET_SIZE = 64;

// Huge test
const uint64_t TOTAL_PAGES = 67108864; // 64 GB
const uint64_t CACHE_PAGES = 4194304; // 4 GB

// Tiny test
//const uint64_t TOTAL_PAGES = 2048; // 2 MB
//const uint64_t CACHE_PAGES = 256; // 256 KB

const uint64_t NUM_SETS = CACHE_PAGES / SET_SIZE;

int ts_counter = 0;

class cache_line
{
	public:
	bool valid;
	bool dirty;
	uint64_t tag;
	uint64_t data;
	int ts;

	cache_line() : valid(false), dirty(false), tag(0), data(0), ts(0) {}
	string str() { stringstream out; out << "T=" << tag << " D=" << data << " V=" << valid << " D=" << dirty << " ts=" << ts; return out.str(); }

};

unordered_map<uint64_t, cache_line> cache;
unordered_map<uint64_t, uint64_t> flash;

string op_name(uint64_t op)
{
	if (op == OP_READ)
		return "READ";
	else if (op == OP_WRITE)
		return "WRITE";
	else
		return "INVALID";
}

uint64_t transaction(uint64_t op, uint64_t addr, uint64_t data)
{
	cout << op_name(op) << " " << addr << " " << data << endl;

	ts_counter += 1;

	// Check vaidity of address
	if (addr % PAGE_SIZE != 0)
	{
		cout << "ERROR: Non-page aligned address" << endl;
		exit(1);
	}
	if (addr >= (TOTAL_PAGES * PAGE_SIZE))
	{
		cout << "ERROR: Address out of bounds" << endl;
		exit(1);
	}

	// Compute page number
	uint64_t page_number = addr / PAGE_SIZE;

	// Compute the set number and tag
	uint64_t set_index = page_number % NUM_SETS;
	uint64_t tag = page_number / NUM_SETS;

	// Compute all cache addresses for this set.
	list<uint64_t> set_address_list;
	//cout << "set_address_list: ";
	for (uint64_t i=0; i<SET_SIZE; i++)
	{
		uint64_t next_address = i * NUM_SETS + set_index;
		set_address_list.push_back(next_address);
		//cout << next_address << " ";
	}
	//cout << endl;

	// Scan teh cache for this address.
	cout << "Scanning cache set " << set_index << " for tag " << tag << "...\n";
	bool hit = false;
	uint64_t cache_address;
	uint64_t cur_address;
	cache_line cur_line;
	for (list<uint64_t>::iterator it = set_address_list.begin(); it != set_address_list.end(); ++it)
	{
		cur_address = *it;
		if (cache.count(cur_address) == 0)
		{
			// If i is not allocated yet, allocate it.
			cache[cur_address] = *(new cache_line());
		}

		cur_line = cache[cur_address];

		//cout << cur_address << " " << hit << " " << cur_line.str() << endl;

		if (cur_line.valid && (cur_line.tag == tag))
		{
			hit = true;
			cache_address = cur_address;
			cout << "FOUND: " << cur_address << " " << hit << " " << cur_line.str() << endl;
			break;
		}

	}

	cout << "page_number " << page_number << endl;
	cout << "set_index " << set_index << endl;
	cout << "tag " << tag << endl;

	if (hit)
	{
		cout << "HIT: cache_address " << cache_address << endl;
	}

	if (!hit)
	{
		// Select a victim offset within the set (LRU)
		uint64_t victim = *(set_address_list.begin());
		int min_ts = -1;

		for (list<uint64_t>::iterator it=set_address_list.begin(); it != set_address_list.end(); it++)
		{
			cur_address = *it;
			cache_line cur_line = cache[cur_address];
			if ((cur_line.ts < min_ts) || (min_ts == -1))
			{
				min_ts = cur_line.ts;
				victim = cur_address;	
			}
		}

		cache_address = victim;

		cout << "MISS: victim is cache_address " << cache_address << endl;


		cur_line = cache[cache_address];

		if (cur_line.dirty)
		{
			// Perform writeback
			uint64_t victim_flash_addr = (cur_line.tag * NUM_SETS + set_index) * PAGE_SIZE;
			flash[victim_flash_addr] = cur_line.data;
			cout << "writeback cache_address= " << cache_address << " (flash_addr, data)=(";
			cout << victim_flash_addr << ", " << cur_line.data << ")\n";
		}

		if (op == OP_READ)
		{
			// Load the page from flash
			cur_line.data = flash[addr];
			cur_line.tag = tag;
			cur_line.dirty = false;
			cur_line.valid = true;
			cur_line.ts = ts_counter;
			cache[cache_address] = cur_line;
		}
	}

	if (op == OP_WRITE)
	{
		cur_line.data = data;
		cur_line.tag = tag;
		cur_line.dirty = true;
		cur_line.valid = true;
		cur_line.ts = ts_counter;
		cache[cache_address] = cur_line;
	}

	cout << endl;

	return cur_line.data;

}

int main()
{

	srand((unsigned int)time(NULL));

	// Basic test
	//transaction(OP_WRITE, 0, 5);
	//cout << transaction(OP_READ, 0, 0) << endl;

	uint64_t TESTS = 1000000;
	list<uint64_t> address_list;
	//uint64_t TESTS = TOTAL_PAGES;
	for (uint64_t i=0; i<TESTS; i++)
	{
		uint64_t page_number = (uint64_t)rand() % TOTAL_PAGES;
		uint64_t address = page_number * PAGE_SIZE;
		address_list.push_back(address);
		transaction(OP_WRITE, address, address);
	}
	for (list<uint64_t>::iterator it = address_list.begin(); it != address_list.end(); ++it)
	{
		uint64_t i = *it;
		uint64_t x = transaction(OP_READ, i, 0);
		if (i != x)
		{
			cout << "FAILED " << i << " " << x << endl;
			exit(1);
		}
	}
	cout << "PASSED" << endl;

	return 0;
}
