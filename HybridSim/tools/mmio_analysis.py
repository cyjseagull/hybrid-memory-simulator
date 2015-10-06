import sys

SET_SIZE = 64
PAGE_SIZE = 4096
CACHE_PAGES = 131072
TOTAL_PAGES = 2097152
BURST_SIZE = 64

HALF_GIG = 2**29
TOTAL_BYTES = TOTAL_PAGES * PAGE_SIZE + HALF_GIG

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

tracefile = open(sys.argv[1], 'r')

count = {}
for i in range(TOTAL_BYTES / HALF_GIG):
	count[i] = 0

max_address = 0

j = 0
all_fs = 0
other_large = 0
other_large_list = []

while(1):
	# Parse line
	line = tracefile.readline()
	if line == '':
		break
	[cycle, op, address] = [int(i) for i in line.strip().split()]

	# Check for illegal address
	if address == 0xFFFFFFFFFFFFFFFF:
		all_fs += 1
		address = ALIGN(address)
	elif address >= TOTAL_BYTES: 
		other_large += 1
		other_large_list.append(address)
		address = ALIGN(address)

	# Track address range
	count_index = address / HALF_GIG
	try:
		count[count_index] += 1
	except Exception, e:
		print address
		print count_index
		raise e

	# Track Max Address
	if address > max_address and address != 0xFFFFFFFFFFFFFFFF:
		max_address = address

	# Print Progress
	j += 1
	if j % 100000 == 0:
		print j

print 'counts',count
print 'All F addresses:',all_fs
print 'Other large',other_large, other_large_list
print 'max_address',max_address

tracefile.close()
