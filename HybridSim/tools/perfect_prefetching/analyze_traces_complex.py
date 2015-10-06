# This tool generates a very detailed analysis of prefetch data from a HybridSim format trace.
# THIS IS NOT A VALID PREFETCH DATA FORMAT. IT IS SIMPLY MEANT FOR STUDY.


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


def process_tracefile(filename):
	tracefile = open(filename, 'r')


	counter = 0
	cnt = {}
	cache = {}
	prefetch = {}
	init = {}

	for i in range(NUM_SETS):
		cnt[i] = 0 # This counts the access number to each set (which is used to trigger prefetches)
		cache[i] = [] # This holds SET_SIZE pairs of (access number, page) in LRU order (the one at the end gets evicted on misses).
		prefetch[i] = [] # This holds triples of (access number, old page, new page) to prefetch. These are triggered immediately after a set is no longer needed.
		init[i] = [] # The initial pages in each cache set.


	# The general algorithm here is that whenever a miss occurs, we know when a particular evicted page is no longer needed (by using its access number).
	# Since we have the last access number a particular page was actually used, we can evict the page immediately after that access rather than waiting
	# until later.
	# The prefetch lists (one for each set) contains all of the data necessary to do this early eviction and prefetching.

	while(1):
		line = tracefile.readline()
		if line == '':
			break

		[cycle, op, address] = [int(i) for i in line.strip().split()]
		address = ALIGN(address)

		page = PAGE_ADDRESS(address)
		set_index = SET_INDEX(address)


		# Check for a hit.
		set_pages = [i for (i,j,k) in cache[set_index]]
		if set_pages.count(page) == 1:
			# We hit.

			# Find and delete the old cache entry for this page.
			page_index = set_pages.index(page)
			del cache[set_index][page_index]

		else:
			# We missed.

			if len(cache[set_index]) == SET_SIZE:
				# Evict the last thing in the cache (index 63).
				evicted = cache[set_index].pop(63)
				(evicted_page, access_number, old_cycle) = evicted

				# Update the prefetch list.
				prefetch[set_index].append((access_number, evicted_page, page, cnt[set_index], old_cycle, cycle))

			else:
				# The cache set isn't full yet, so just put this at the front of the list and do not evict anything.
				init[set_index].append(page)
			
		# Insert the new entry at the beginning of the list (for LRU).
		cache[set_index].insert(0, (page, cnt[set_index], cycle))


		# Increment the counter for the current set.
		cnt[set_index] += 1
		counter += 1

		if counter % 100000 == 0:
			print counter

	# Done. Now write this to a file.
	min_diff = 100000000000 # Just start with a large number
	min_access_diff = 100000000000 # Just start with a large number
	outFile = open('prefetch_data.txt', 'w')
	outFile.write('NUM_SETS '+str(NUM_SETS)+'\n\n\n')
	for i in range(NUM_SETS):
		outFile.write('SET '+str(i)+' '+str(len(prefetch[i]))+'\n')
	#	outFile.write(str(prefetch[i])+'\n\n')
		for j in range(len(prefetch[i])):
			access_number = str(prefetch[i][j][0])
			evicted_page = str(prefetch[i][j][1])
			prefetch_page = str(prefetch[i][j][2])
			new_access_number = str(prefetch[i][j][3])
			old_cycle = str(prefetch[i][j][4])
			new_cycle = str(prefetch[i][j][5])
			cycle_diff = int(new_cycle) - int(old_cycle)
			access_diff = int(new_access_number) - int(access_number)
			if cycle_diff < min_diff:
				min_diff = cycle_diff
			if access_diff < min_access_diff:
				min_access_diff = access_diff
			outFile.write('accesses:('+access_number+', '+new_access_number+') pages:('+evicted_page+', '+prefetch_page+') cycles:('+old_cycle+', '+new_cycle+') diff: '+str(cycle_diff)+'\n');
		outFile.write('\n\n')
	outFile.close()
	print 'min_diff:',min_diff
	print 'min_access_diff:',min_access_diff


	# Save cache state table.
	outFile = open('prefetch_cache_state.txt', 'w')
	outFile.write(str(PAGE_SIZE)+' '+str(SET_SIZE)+' '+str(CACHE_PAGES)+' '+str(TOTAL_PAGES)+'\n')
	for i in range(CACHE_PAGES):
		cache_addr = i*PAGE_SIZE
		set_index = SET_INDEX(cache_addr)

		# If there is another address to output in this file.
		if len(init[set_index]) > 0:
			cur_page = init[set_index].pop(0)
			tag = TAG(cur_page)
			ts = i/NUM_SETS

			# order to output is cache_addr 1 1 tag 0 ts
			outFile.write(str(cache_addr)+' 1 1 '+str(tag)+' 0 '+str(ts)+'\n')

	outFile.close()


process_tracefile(sys.argv[1])

