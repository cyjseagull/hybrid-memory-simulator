# This script compares the prefetch_cache_state files from multiple runs of marss.
# It tells how many values in each cache set are common across all runs.

import sys

SET_SIZE = 64
PAGE_SIZE = 4096
CACHE_PAGES = 131072
TOTAL_PAGES = 2097152
BURST_SIZE = 64

NUM_SETS = CACHE_PAGES / SET_SIZE

def PAGE_NUMBER(addr):
	return addr / PAGE_SIZE
def PAGE_ADDRESS(addr):
	return ((addr / PAGE_SIZE) * PAGE_SIZE)
def PAGE_OFFSET(addr):
	return (addr % PAGE_SIZE)
def SET_INDEX(addr):
	return (PAGE_NUMBER(addr) % NUM_SETS)
def TAG(addr):
	return (PAGE_NUMBER(addr) / NUM_SETS)
def FLASH_ADDRESS(tag, set_num):
	return ((tag * NUM_SETS + set_num) * PAGE_SIZE)
def ALIGN(addr):
	return (((addr / BURST_SIZE) * BURST_SIZE) % (TOTAL_PAGES * PAGE_SIZE))

# Load all states into memory
state = {}
for i in range(1,6):
	state[i] = {}
	for j in range(NUM_SETS):
		state[i][j] = []
	inFile = open(str(i)+'/prefetch_cache_state.txt', 'r')

	# Skip first line
	line = inFile.readline()

	while True:
		line = inFile.readline()
		if line == '':
			break

		[cache_addr, valid, dirty, tag, data, ts] = [int(k) for k in line.strip().split()]

		set_index = SET_INDEX(cache_addr)

		state[i][set_index].append(tag)

# Count the values in the first set that are in all other sets
set_count = []
for j in range(NUM_SETS):
	cur_count = 0
	for k in range(len(state[1][j])):
		cur_val = state[1][j][k]

		# See if the value is in all other sets.
		in_all = True
		if cur_val not in state[2][j]:
			in_all = False
		if cur_val not in state[3][j]:
			in_all = False
		if cur_val not in state[4][j]:
			in_all = False
		if cur_val not in state[5][j]:
			in_all = False

		if in_all:
			cur_count += 1

	set_count.append(cur_count)
	
#print set_count

set_count_count = {}
for k in set_count:
	if k not in set_count_count.keys():
		set_count_count[k] = 1
	else:
		set_count_count[k] += 1
		
print set_count_count
