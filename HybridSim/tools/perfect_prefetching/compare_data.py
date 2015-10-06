# This script is used to compare prefetch data from multiple runs of marss.
# It is currently configured to compare 5 runs that are in subfolders 1,2,3,4,5.
# It prints out an analysis of variance for when victims are evicted from the cache between runs.

import sys
import math

NUM_SETS = 2048

data = {}
set_fails = {}
max_diff = 0
total_diff = 0
victim_cnt = 0
diff_list = []

# Load all data into memory
for i in range(1,6):
	print 'Processing file',i
	data[i] = {}
	inFile = open(str(i)+'/prefetch_data.txt', 'r')

	# Double check that the number of sets for this file matches.
	num_sets = int(inFile.readline().strip().split()[1])
	if num_sets != NUM_SETS:
		print 'ERROR! Number of sets does not match.'
		sys.exit(1)


	for j in range(NUM_SETS):
		# Consume two lines.
		tmp = inFile.readline()
		tmp = inFile.readline()

		data[i][j] = {} # This set maps from the address being EVICTED and the access number eviction is triggered on.
		set_fails[j] = []
	
		# Parse the set declaration line.
		[tmp, set_num, set_cnt] = inFile.readline().strip().split()
		set_num = int(set_num)
		if set_num != j:
			print 'ERROR! Sets do not appear sequentially. Expected',j,'and saw',set_num
			sys.exit(1)
		set_cnt = int(set_cnt)

		# Parse each line.
		for k in range(set_cnt):
			[access_number, victim_page, new_page] = [int(m) for m in inFile.readline().strip().split()]
			if victim_page not in data[i][j]:
				data[i][j][victim_page] = access_number
			else:
				print 'WARNING! The victim',victim_page,'appears more than once in set',set_num
				sys.exit(1)

			if i == 5:
				# Analysis code

				fail = False
				for m in range(1,5):
					# This is a "set fail" if it is not in all five runs.
					if victim_page not in data[m][j]:
						#print 'WARNING! The victim',victim_page,'in set',set_num,'does not appear in file',m
						#sys.exit(1)
						set_fails[j].append(victim_page)
						fail = True
						break

				if not fail:
					max_access = 0
					min_access = 1000000000000

					for m in range(1,6):
						cur_val = data[m][j][victim_page]
						if cur_val > max_access:
							max_access = cur_val
						if cur_val < min_access:
							min_access = cur_val

					cur_diff = max_access - min_access
					if cur_diff > max_diff:
						max_diff = cur_diff

					total_diff += cur_diff
					victim_cnt += 1
					diff_list.append(cur_diff)

print 'set_fails',sum([len(set_fails[j]) for j in range(NUM_SETS)])
print 'max_diff',max_diff
print 'avg_diff',total_diff/float(victim_cnt)
diff_list.sort()
print 'median diff',diff_list[len(diff_list)/2]
print 'victim_cnt',victim_cnt

outFile = open('diff_list.txt', 'w')
for i in diff_list:
	outFile.write(str(i)+'\n')
outFile.close()

above_10 = 0
above_50 = 0
above_100 = 0
above_200 = 0
above_300 = 0
above_400 = 0
for i in diff_list:
	if i > 10:
		above_10 += 1
	if i > 50:
		above_50 += 1
	if i > 100:
		above_100 += 1
	if i > 200:
		above_200 += 1
	if i > 300:
		above_300 += 1
	if i > 400:
		above_400 += 1
print 'above_10',above_10
print 'above_50',above_50
print 'above_100',above_100
print 'above_200',above_200
print 'above_300',above_300
print 'above_400',above_400
