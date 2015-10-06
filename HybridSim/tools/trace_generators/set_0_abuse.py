import sys

ADJUSTMENT = 400 	# Set the adjustment to prevent premature evictions in non-deterministic runs (e.g. marss)
					# If fully deterministic, set this to 0.

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

for i in range(1000000):
	curtag = i % (TOTAL_PAGES / NUM_SETS)
	flash_addr = FLASH_ADDRESS(curtag, 0)
	print '10 1 ',flash_addr
