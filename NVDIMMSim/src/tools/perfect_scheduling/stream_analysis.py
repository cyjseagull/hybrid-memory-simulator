#*********************************************************************************
#  Copyright (c) 2011-2012, Paul Tschirhart
#                             Jim Stevens
#                             Peter Enns
#                             Ishwar Bhati
#                             Mu-Tien Chang
#                             Bruce Jacob
#                             University of Maryland 
#                             pkt3c [at] umd [dot] edu
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  
#     * Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#  
#     * Redistributions in binary form must reproduce the above copyright notice,
#        this list of conditions and the following disclaimer in the documentation
#        and/or other materials provided with the distribution.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#********************************************************************************

import sys
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# capacity parameters
NUM_PACKAGES = 32
DIES_PER_PACKAGE = 2
PLANES_PER_DIE = 1
BLOCKS_PER_PLANE = 32
PAGES_PER_BLOCK = 48
NV_PAGE_SIZE=32768 # in bits

file_out = 0
write_arrive = 0
read_arrive = 0
per_plane = 0
image_out = "StreamAnalysis.pdf"
epoch_size = 100000

# get the log files
plane_log = open(sys.argv[1], 'r')

if len(sys.argv) == 8:
	image_out = sys.argv[2]
	epoch_size = int(sys.argv[3])
	per_plane = int(sys.argv[4])
	write_arrive = 1
	write_log = open(sys.argv[5], 'r')
	read_arrive = 1
	read_log = open(sys.argv[6], 'r')
	file_out = 1 # just so we know later to write to a file
	output_file = open(sys.argv[7], 'w')
elif len(sys.argv) == 7:
	image_out = sys.argv[2]
	epoch_size = int(sys.argv[3])
	per_plane = int(sys.argv[4])
	write_arrive = 1
	write_log = open(sys.argv[5], 'r')
	read_arrive = 1
	read_log = open(sys.argv[6], 'r')
elif len(sys.argv) == 6:
	image_out = sys.argv[2]
	epoch_size = int(sys.argv[3])
	per_plane = int(sys.argv[4])
	write_arrive = 1
	write_log = open(sys.argv[5], 'r')
elif len(sys.argv) == 5:
	image_out = sys.argv[2]
	epoch_size = int(sys.argv[3])
	per_plane = int(sys.argv[4])
elif len(sys.argv) == 4:
	image_out = sys.argv[2]
	epoch_size = int(sys.argv[3])
elif len(sys.argv) == 3:
	image_out = sys.argv[2]
	

# preparse everything into files
plane_data = []
write_data = []
read_data = []

# just so we can initialize the epochs arrays we need to know how many epochs to expect
plane_last_clock = 0
write_last_clock = 0
read_last_clock = 0

# get all the plane log data
while(1):	
	state = plane_log.readline()
	# if the state is blank we've reached the end of the file
	if state == '':
		break
	
	if state == 'Plane State Log \n':
		#do nothing for now
		print 'starting plane state parsing'
		continue

	plane_data.append(state)
	[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]
	plane_last_clock = state_cycle
	
# get all the write arrive log data
if write_arrive == 1:
	while(1):
		write = write_log.readline()
		
		if write == '':
			break

		elif write == 'Write Arrival Log \n':
		#do nothing for now
			print 'starting write arrival parsing'
			continue

		write_data.append(write)
		[tcycle, address] = [int(i) for i in write.strip().split()]
		write_last_clock = tcycle

# get all the read arrive log data
if read_arrive == 1:
	while(1):
		read = read_log.readline()
		
		if read == '':
			break

		elif read == 'Read Arrival Log \n':
		#do nothing for now
			print 'starting read arrival parsing'
			continue

		read_data.append(read)
		[tcycle, address] = [int(i) for i in read.strip().split()]
		read_last_clock = tcycle

epoch = 0
last_clock = write_last_clock if write_last_clock > plane_last_clock else plane_last_clock
real_last_clock = last_clock if last_clock > read_last_clock else read_last_clock
epoch_total = int(real_last_clock / epoch_size) + 1
epoch_writes = [0 for k in range(epoch_total)]
epoch_reads = [0 for k in range(epoch_total)]
epoch_writes_arrived = [0 for k in range(epoch_total)]
epoch_reads_arrived = [0 for k in range(epoch_total)]

pp_epoch_writes = [[[[0 for x in range(epoch_total)] for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
pp_epoch_reads = [[[[0 for x in range(epoch_total)] for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]

write_pointer = 0
read_pointer = 0
plane_pointer = 0
writes_done = 0
reads_done = 0
planes_done = 0

while(1):
	if write_arrive == 1:
		#count the arriving writes in this epoch
		while(1):
			# check to see if we're done
			if write_pointer >= len(write_data):
				writes_done = 1
				break

			curr_write = write_data[write_pointer]
			[tcycle, address] = [int(i) for i in curr_write.strip().split()]
			if tcycle < ((epoch + 1) * epoch_size):
				epoch_writes_arrived[epoch] = epoch_writes_arrived[epoch] + 1
			else:
				if file_out == 1:
					s =  str(epoch)
					output_file.write(s)
					output_file.write(" ")
					s =  str(epoch_writes_arrived[epoch])
					output_file.write(s)
					output_file.write(" ")
					break
				
				write_pointer = write_pointer + 1;

	if read_arrive == 1:
	#count the arriving reads in this epoch
		while(1):
			# check to see if we're done
			if read_pointer >= len(read_data):
				reads_done = 1
				break
			
			curr_read = read_data[read_pointer]
			[tcycle, address] = [int(i) for i in curr_read.strip().split()]
			if tcycle < ((epoch + 1) * epoch_size):
				epoch_reads_arrived[epoch] = epoch_reads_arrived[epoch] + 1
			else:
				if file_out == 1:
					s =  str(epoch)
					output_file.write(s)
					output_file.write(" ")
					s =  str(epoch_reads_arrived[epoch])
					output_file.write(s)
					output_file.write(" ")
					break
				
				read_pointer = read_pointer + 1;

	#count the current reads and writes in this epoch
	while(1):
		# check to see if we're done
		if plane_pointer >= len(plane_data):
			planes_done = 1
			break

		curr_read = plane_data[plane_pointer]
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_read.strip().split()]
		if state_cycle < ((epoch + 1) * epoch_size):
			if op == 1:
				if per_plane == 1:
					pp_epoch_reads[package][die][plane][epoch] = pp_epoch_reads[package][die][plane][epoch] + 1
				epoch_reads[epoch] = epoch_reads[epoch] + 1
			elif op == 3:
				if per_plane == 1:
					pp_epoch_writes[package][die][plane][epoch] = pp_epoch_writes[package][die][plane][epoch] + 1
				epoch_writes[epoch] = epoch_writes[epoch] + 1
		else:
			if file_out == 1:
				s =  str(epoch_reads[epoch])
				output_file.write(s)
				output_file.write(" ")
				s =  str(epoch_writes[epoch])
				output_file.write(s)
				output_file.write("\n")
			break

		plane_pointer = plane_pointer + 1;
	
	if planes_done:
		break
	
	epoch = epoch + 1

#Now we have two full arrays to graph so lets graph them
epoch_list = range(epoch_total)
plt.figure(0)
plt.plot(epoch_list, epoch_reads, label = "Reads")
plt.plot(epoch_list, epoch_writes, label = "Writes")
if write_arrive == 1:
	plt.plot(epoch_list, epoch_writes_arrived, label = "Writes Arrived")
if read_arrive == 1:
	plt.plot(epoch_list, epoch_reads_arrived, label = "Reads Arrived")
plt.legend()
actual_image_out = image_out + ".pdf"
plt.savefig(actual_image_out)

print per_plane
if per_plane == 1:
	count = 1
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				plt.figure(count)
				plt.plot(epoch_list, pp_epoch_reads[i][j][k], label = "Reads")
				plt.plot(epoch_list, pp_epoch_writes[i][j][k], label = "Writes")
				
				plt.legend()
				temp_name =   image_out + "-Pack" + str(i) + "-Die" + str(j) + "-Plane" + str(k) + ".pdf"
				print temp_name
				plt.savefig(temp_name)
				count = count + 1
