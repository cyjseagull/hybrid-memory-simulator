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

# capacity parameters
NUM_PACKAGES = 32
DIES_PER_PACKAGE = 4
PLANES_PER_DIE = 1
BLOCKS_PER_PLANE = 32
PAGES_PER_BLOCK = 48
NV_PAGE_SIZE=32768 # in bits

# timing parameters
DEVICE_CYCLE = 2.5
CHANNEL_CYCLE = 0.15
DEVICE_WIDTH = 8
CHANNEL_WIDTH = 8

READ_CYCLES = 16678 # pretty sure I don't need this actually
WRITE_CYCLES = 133420
ERASE_CYCLES = 1000700
COMMAND_LENGTH = 56

# just for read analysis
PCM_WRITE_CYCLES = 47020
DRAM_WRITE_CYCLES = 90

CYCLE_TIME = 1.51

# derived parameters

TOTAL_PLANES = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE
CONCURRENCY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE
CAPACITY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK

CYCLES_PER_TRANSFER = NV_PAGE_SIZE / CHANNEL_WIDTH

READ_TIME = (READ_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
WRITE_TIME = (WRITE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
ERASE_TIME = (ERASE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME

# get the log files
write_log = open(sys.argv[1], 'r')
plane_log = open(sys.argv[2], 'r')
# are we just getting read statistics or are we trying to schedule writes
mode = sys.argv[3]

# see if we're in error checking mode
if len(sys.argv) == 7:
	file_out = 1 # just so we know later to write to a fill
	print sys.argv[6]
	error_file = open(sys.argv[6], 'w')
	error_out = 1 # just so we know we're printing errors
	if sys.argv[4] == 'Append':
		output_file = open(sys.argv[5], 'a')
	else:
		output_file = open(sys.argv[5], 'w')
# see if we've specified whether or not we want to append to this file or not
if len(sys.argv) == 6:
	file_out = 1 # just so we know later to write to a file
	if sys.argv[4] == 'Append':
		output_file = open(sys.argv[5], 'a')
	else:
		output_file = open(sys.argv[5], 'w')
# if we didn't but we still provided an output file assume we're not appending
elif len(sys.argv) == 5:
	file_out = 1 # just so we know later to write to a file
	output_file = open(sys.argv[4], 'w')
	

# preparse everything into files
plane_data = []
write_data = []

#========================================================================================================
# Read Analysis
#========================================================================================================
# analyzing reads only to find out the space between them
if mode == 'Read':
	# idle time records
	shortest_time = READ_TIME * WRITE_TIME #just a big number
	longest_time = 0
	average_time = 0
	average_times = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	# read gap records
	bigger_count = 0
	open_count = 0
	short_count = 0
	shorter_count = 0
	lookup_count = 0
	one_count = 0
	# potential write records
	open_counts = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	last_read = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	idle = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	idle_counts = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	data_counter = 0

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

	#read in the plane states log
	#find the state of the planes up to this time
	while(1):
		# check to see if we're done
		if data_counter >= len(plane_data):
			break

		curr_data = plane_data[data_counter]
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_data.strip().split()]
		# if this is a record of a plane going idle record that plane as now idle and this cycle as the end
		# of its last read
		if op == 0:
			idle[package][die][plane] = 1
			last_read[package][die][plane] = state_cycle
		
		# if this is a record of a plane starting a read and that plane was idle then compute the amount of time
		# this plane was idle
		if idle[package][die][plane] == 1 and op != 0:
			temp_time = state_cycle - last_read[package][die][plane]
			if temp_time < shortest_time:
				shortest_time = temp_time
			elif temp_time > longest_time:
				longest_time = temp_time

			# was this plane idle long enough that it could have done a write?
			if temp_time > WRITE_CYCLES:
				open_count = open_count + math.floor(temp_time/WRITE_CYCLES)
				open_counts[package][die][plane] = open_counts[package][die][plane] + 1
				if temp_time > WRITE_CYCLES + 50:
					bigger_count = bigger_count + 1
			elif temp_time > PCM_WRITE_CYCLES:
				short_count = short_count + 1
			elif temp_time > DRAM_WRITE_CYCLES:
				shorter_count = shorter_count + 1
			elif temp_time > 25:
				lookup_count = lookup_count + 1
			elif temp_time >= 1:
				one_count = one_count + 1
				
			average_times[package][die][plane] = average_times[package][die][plane] + temp_time
			idle_counts[package][die][plane] = idle_counts[package][die][plane] + 1
		# done with that line of the log, move on to the next one
		data_counter =  data_counter + 1
			
	# add everything up for the total average across all planes		
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):	
				if idle_counts[i][j][k] > 0:			
					average_times[i][j][k] = average_times[i][j][k] / idle_counts[i][j][k]
					average_time = average_time + average_times[i][j][k]

	# divide to get the average
	average_time = average_time / TOTAL_PLANES
				
	print 'shortest idle time', shortest_time
	print 'longest idle time', longest_time
	print 'average idle time', average_time
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				print 'average time for package', i, 'die', j, 'plane', k, 'is', average_times[i][j][k]
	print 'number of idle times large enough for a write and then some', bigger_count
	print 'number of idle times large enough for a write', open_count
	print 'number of idle times less than a PCM write', short_count
	print 'number of idle times less than a DRAM write', shorter_count
	print 'number of idle times less than a lookup time', lookup_count
	print 'number of idle times one 1 cycle long', one_count
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				print 'write sized gaps for package', i, 'die', j, 'plane', k, 'is', open_counts[i][j][k]

	# save stuff to a file
	if file_out == 1:
		output_file.write('===================\n')
		output_file.write('=  Read Analysis  =\n')
		output_file.write('===================\n')
		#annoying
		output_file.write('shortest idle time ')
		s =  str(shortest_time)
		output_file.write(s)
		output_file.write('\n')		
		
		output_file.write('longest idle time ')
		s =  str(longest_time)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('average idle time ')
		s =  str(average_time)
		output_file.write(s)
		output_file.write('\n')	

		for i in range(NUM_PACKAGES):
			for j in range(DIES_PER_PACKAGE):
				for k in range(PLANES_PER_DIE):
					output_file.write('average time for package ')
					s =  str(i)
					output_file.write(s)
					output_file.write(' die ')
					s =  str(j)
					output_file.write(s)
					output_file.write(' plane ')
					s =  str(k)
					output_file.write(s)
					output_file.write(' is ')
					s =  str(average_times[i][j][k])
					output_file.write(s)
					output_file.write('\n')	
		output_file.write('number of idle times large enough for a write and then some ')
		s =  str(bigger_count)
		output_file.write(s)
		output_file.write('\n')
		output_file.write('number of idle times large enough for a write ')
		s =  str(open_count)
		output_file.write(s)
		output_file.write('\n')
		output_file.write('number of idle times less than a PCM write ')
		s =  str(short_count)
		output_file.write(s)
		output_file.write('\n')
		output_file.write('number of idle times less than a DRAM write ')
		s =  str(shorter_count)
		output_file.write(s)
		output_file.write('\n')
		output_file.write('number of idle times less than a lookup time ')
		s =  str(lookup_count)
		output_file.write(s)
		output_file.write('\n')
		output_file.write('number of idle times one 1 cycle long ')
		s =  str(one_count)
		output_file.write(s)
		output_file.write('\n')
		for i in range(NUM_PACKAGES):
			for j in range(DIES_PER_PACKAGE):
				for k in range(PLANES_PER_DIE):
					output_file.write('write sized gaps for package ')
					s =  str(i)
					output_file.write(s)
					output_file.write(' die ')
					s =  str(j)
					output_file.write(s)
					output_file.write(' plane ')
					s =  str(k)
					output_file.write(s)
					output_file.write(' is ')
					s =  str(open_counts[i][j][k])
					output_file.write(s)
					output_file.write('\n')	

#========================================================================================================
# Write Analysis
#========================================================================================================
# trying to place writes between the reads and determining if our actions delay either
elif mode == 'Write':

	# with the perfect scheduling version of plane state we do need to keep track of the planes
	planes = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]

	# list of pending writes that this analysis creates
	pending_writes = []

	# list of times when channels will be free
	busy_channels = []

	# list of times when channels will be used
	soon_channels = []

	# list of the addresses of pending writes to make sure that a read doesn't go through before its corresponding write
	pending_addresses = {} #dictionary cycle:address

	cycle = 0
	delayed_writes = 0
	delayed_reads = 0
	completed_writes = 0
	completed_reads = 0
	free_planes = TOTAL_PLANES
	free_channels = NUM_PACKAGES
	write_delayed = 0
	read_delayed = 0
	channel_delays = 0
	RAW_haz = 0 

	#reading counters
	write_counter = 0
	plane_counter = 0
	
	# get all the write log data
	while(1):
		write = write_log.readline()

		if write == '':
			break

		elif write == 'Write Arrival Log \n':
			#do nothing for now
			print 'starting write arrival parsing'
			continue
	
		write_data.append(write)
	
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
	
	while(1):
		if write_delayed == 0:
			# see if we're done
			if write_counter >= len(write_data):
				break

			# parse the write data
			curr_write = write_data[write_counter]
			[tcycle, address] = [int(i) for i in curr_write.strip().split()]
			if tcycle > cycle:
				cycle = tcycle
			# because we can increment the cycles here we kind wind up with two writes
			# occuring on the same cycle which can't happen so just increment once here
			# to make sure that won't happen
			else:
				cycle = cycle + 1
				delayed_writes = delayed_writes + 1	

			# we can move on
			write_counter = write_counter + 1
			#print write_counter	
		else:
			# increment the cycle count
			cycle = cycle + 1
	
		#find the state of the planes up to this time
		while(1):
			# see if we're done here too
			if plane_counter >= len(plane_data):
				break
			
			# parse the read data
			curr_op = plane_data[plane_counter]
			[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_op.strip().split()]
	
			# if the cycle of this state change is greater than the write arrival cycle
			# break cause we're not here yet
			if state_cycle > cycle:
				break

			# if the cycle of this state change is the same or greater than the cycle of when a
			# read was to start transfering, start the transfer
			for t in soon_channels:
				if t >= cycle:
					free_channels = free_channels - 1
					soon_channels.remove(t)
		
					# if this read needed a channel to transfer its data and we didn't have it then
					# it would have been delayed so record that
					if free_channels < 0:
						delayed_reads = delayed_reads + 1
						channel_delays = channel_delays + 1
										

			# if the plane is newly idle update the planes count to reflect a new open
			# plane
			if planes[package][die][plane] != 0 and op == 0:
				free_planes = free_planes + 1
				# channel is now no longer being used
				free_channels = free_channels + 1
				completed_reads = completed_reads + 1
			elif planes[package][die][plane] == 0 and op != 0:
				free_planes = free_planes - 1	
				# let us know that a channel will go busy when this read is done
				soon_channels.append(cycle + READ_CYCLES)			
				
				# if this read needed a plane that we didn't have then it would have 
				# been delayed so record that
				if free_planes < 0:
					delayed_reads = delayed_reads + 1
	
				# if this read was for a write that hasn't yet finished, record the error
				if state_address in pending_addresses:
					delayed_reads = delayed_reads + 1
					RAW_haz = RAW_haz + 1

			planes[package][die][plane] = op

			# got to the end so we're good to move on the next read record
			plane_counter = plane_counter + 1					
			
		# bug hunting
		#if write_counter > 11000 and error_out == 1:
		#	error_file.write('Pending Writes ')
		#	s = str(len(pending_writes))
		#	error_file.write(s)
		#	error_file.write('\n')
		#	error_file.write('Pending Addresses ')
		#	s = str(len(pending_addresses))
		#	error_file.write(s)
		#	error_file.write('\n')
		#check to see if any pending writes are done
		for p in pending_writes:
			if p <= cycle:	
				if write_counter > 11000 and error_out == 1:
					error_file.write('Removed cycle ')
					s = str(p)
					error_file.write(s)
					error_file.write('\n')
					error_file.write('Corresponding Address ')
					s = str(pending_addresses[p])
					error_file.write(s)
					error_file.write('\n')				
				free_planes = free_planes + 1
				pending_writes.remove(p)
				del pending_addresses[p]
				completed_writes = completed_writes + 1

		#check to see if we're done using a channel to transfer the data
		for c in busy_channels:
			if c <= cycle:
				free_channels = free_channels + 1
				busy_channels.remove(c)
	
		#is there a free plane and channel for this write
		if free_planes == 0 or free_channels == 0 and write_delayed == 0:
			delayed_writes = delayed_writes + 1
			write_delayed = 1
		#issue the write to a plane and determine its completion cycle
		elif free_planes > 0 and free_channels > 0:
			free_planes = free_planes - 1
			pending_writes.append(cycle + WRITE_CYCLES + CYCLES_PER_TRANSFER)
			pending_addresses[cycle + WRITE_CYCLES + CYCLES_PER_TRANSFER] = address
			write_delayed = 0
			# gonna be using the channel immediately
			free_channels = free_channels - 1
			busy_channels.append(cycle + CYCLES_PER_TRANSFER)
			if write_counter > 11000 and error_out == 1:
				error_file.write('Added cycle ')
				s = str(cycle + WRITE_CYCLES + CYCLES_PER_TRANSFER)
				error_file.write(s)
				error_file.write('\n')
				error_file.write('Corresponding Address ')
				s = str(address)
				error_file.write(s)
				error_file.write('\n')		
 		 
	print 'free planes', free_planes
	print 'free channels', free_channels
	print 'delayed reads', delayed_reads
	print 'delayed writes', delayed_writes
	print 'channel delays', channel_delays	
	print 'completed writes', completed_writes
	print 'completed reads', completed_reads
	print 'RAW hazards', RAW_haz

	# save stuff to a file
	if file_out == 1:
		output_file.write('===================\n')
		output_file.write('=  Write Analysis  =\n')
		output_file.write('===================\n')
		#annoying again
		output_file.write('free planes ')
		s =  str(free_planes)
		output_file.write(s)
		output_file.write('\n')		

		output_file.write('free channels ')
		s =  str(free_channels)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('delayed reads ')
		s =  str(delayed_reads)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('delayed writes ')
		s =  str(delayed_writes)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('channel delays ')
		s =  str(channel_delays)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('completed writes ')
		s =  str(completed_writes)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('completed reads ')
		s =  str(completed_reads)
		output_file.write(s)
		output_file.write('\n')	

		output_file.write('RAW hazards ')
		s =  str(RAW_haz)
		output_file.write(s)
		output_file.write('\n')	
#========================================================================================================
# Script Generating Analysis Mode 1
#========================================================================================================
elif mode == 'Sched1':
	# with the perfect scheduling version of plane state we do need to keep track of the planes
	plane_states = [[[[] for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]

	# cycles when each planes next write gap appears and ends
	plane_gaps = [[[[] for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	
	# counters
	write_counter = 0
	read_counters = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	read_counter = 0

	# write pointers
	curr_pack = 0
	curr_die = 0
	curr_plane = 0

	delayed_writes = 0
	placed_writes = 0
	end_writes = 0

	last_read = 0
	write_clock = 0

	RAW_haz = 0

	cycle = 0	
	
	# get all the write log data
	while(1):
		write = write_log.readline()

		if write == '':
			break

		elif write == 'Write Arrival Log \n':
			#do nothing for now
			print 'starting write arrival parsing'
			continue
	
		write_data.append(write)
	
	# get all the plane log data
	# and pre-parse it into per plane queues
	previous_state = []
	previous_read = []
	while(1):
		state = plane_log.readline()
		# if the state is blank we've reached the end of the file
		if state == '':
			break
	
		if state == 'Plane State Log \n':
			#do nothing for now
			print 'starting plane state parsing'
			continue
 
		
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]				
		plane_states[package][die][plane].append([state_cycle, state_address, op])
		plane_data.append(state)

	# now that we have that we want to turn those plane state logs into plane gap logs
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				for h in range(len(plane_states[i][j][k])):
					# if its the first entry we can't have a gap yet
					# technically we could but because the clock cycles are huge and screwed up, 
					# I don't think we can do that right now
					if h == 0:
						print 'starting per plane gap parsing'
					else:
						[state_cycle, state_address, op] = plane_states[i][j][k][h]
						[previous_cycle, previous_address, previous_op] = plane_states[i][j][k][h-1]

						# if this op and the previous one bookend a gap			
						if previous_op == 0 and op != 0 and (state_cycle - previous_cycle) >= WRITE_CYCLES:
							plane_gaps[i][j][k].append([previous_cycle, state_cycle])
	
	still_gaps = 1						
	# go through the writes in order and try to place them
	while(1):	
		# see if we're done here
		if write_counter >= len(write_data):
			break

		# parse the write data
		curr_write = write_data[write_counter]
		[tcycle, address] = [int(i) for i in curr_write.strip().split()]
	
		# we can move on
		write_counter = write_counter + 1

		removals = []
		if still_gaps == 1:							

			# loop through the planes and find the soonest gap and use that one
			soonest_start = sys.maxint
			soonest_pack = 0
			soonest_die = 0
			soonest_plane = 0
			found = 0
			for i in range(NUM_PACKAGES):
				for j in range(DIES_PER_PACKAGE):
					for k in range(PLANES_PER_DIE):
						# make sure the gap list isn't empty
						if len(plane_gaps[i][j][k]) > 0:
							#check the head of each queue of gaps
							[gap_start, gap_end] = plane_gaps[i][j][k][0]
							if gap_start < soonest_start and gap_start >= tcycle and gap_end - tcycle >= WRITE_CYCLES:
								soonest_start = gap_start
								soonest_pack = i
								soonest_die = j
								soonest_plane = k
								found = 1
			
			# check for RAW hazards
			# start at the beginning of the plane log
			while(1):
				# see if we're done here
				if read_counter >= len(plane_data):
					break
	
				[state_cycle, state_address, package, die, plane, op] = [int(z) for z in plane_data[read_counter].strip().split()]	
	
				# if this was a read and it originally happened after the write and it now happens before the write is done
				if op != 0 and state_address == address and state_cycle > tcycle and state_cycle < (soonest_start + WRITE_CYCLES):
					RAW_haz = RAW_haz + 1
	
				# if we're to a plane state that's past our write break
				if state_cycle >= tcycle:
					break
	
				read_counter = read_counter + 1
				last_read = state_cycle			
	
			# pop the read gap cause we've used it
			if found == 1:
				[temp_start, temp_end] = plane_gaps[soonest_pack][soonest_die][soonest_plane][0]
				if temp_end - (temp_start + WRITE_CYCLES) >= WRITE_CYCLES:
					temp_start =  temp_start + WRITE_CYCLES
					plane_gaps[soonest_pack][soonest_die][soonest_plane][0] = [temp_start, temp_end]
				else:
					plane_gaps[soonest_pack][soonest_die][soonest_plane].remove(plane_gaps[soonest_pack][soonest_die][soonest_plane][0])				
				
				placed_writes = placed_writes + 1
				# save the write 
				s = str(soonest_start)
				output_file.write(s)
				output_file.write(' ')
				s = str(address)
				output_file.write(s)
				output_file.write(' ')
				s = str(soonest_pack)		
				output_file.write(s)
				output_file.write(' ')
				s = str(soonest_die)
				output_file.write(s)
				output_file.write(' ')
				s = str(soonest_plane)
				output_file.write(s)
				output_file.write('\n')
		
			# if we didn't find any gaps then out of gaps
			else:
				still_gaps = 0
				print 'ran out of gaps'
			
				if write_counter < CONCURRENCY:
					write_clock = last_read + READ_CYCLES
				else:
					write_clock = write_clock + WRITE_CYCLES
		
				# just incase the writes were legit after all the reads were done
				if write_clock < tcycle:
					write_clock = tcycle
					end_writes = end_writes + 1
				else:
					delayed_writes = delayed_writes + 1
	
				# save the write 
				s = str(write_clock)
				output_file.write(s)
				output_file.write(' ')
				s = str(address)
				output_file.write(s)
				output_file.write(' ')
				s = str(curr_pack)		
				output_file.write(s)
				output_file.write(' ')
				s = str(curr_die)
				output_file.write(s)
				output_file.write(' ')
				s = str(curr_plane)
				output_file.write(s)
				output_file.write('\n')

				# wrap the write pointer back around
				curr_plane = curr_plane + 1
				if curr_plane > PLANES_PER_DIE:
					curr_plane = 0
					curr_die = curr_die + 1
					if curr_die > DIES_PER_PACKAGE:
						curr_die = 0
						curr_pack = curr_pack + 1
						if curr_pack > NUM_PACKAGES:
							curr_pack = 0

		# out of gaps so just add all of the remaining writes to the end of the file in round robin
		else:
			print 'fell through due to lack of gaps'
			if write_counter < CONCURRENCY:
				write_clock = last_read + READ_CYCLES
			else:
				write_clock = write_clock + WRITE_CYCLES
		
			# just incase the writes were legit after all the reads were done
			if write_clock < tcycle:
				write_clock = tcycle
				end_writes = end_writes + 1
			else:
				delayed_writes = delayed_writes + 1
	
			# save the write 
			s = str(write_clock)
			output_file.write(s)
			output_file.write(' ')
			s = str(address)
			output_file.write(s)
			output_file.write(' ')
			s = str(curr_pack)		
			output_file.write(s)
			output_file.write(' ')
			s = str(curr_die)
			output_file.write(s)
			output_file.write(' ')
			s = str(curr_plane)
			output_file.write(s)
			output_file.write('\n')

			# wrap the write pointer back around
			curr_plane = curr_plane + 1
			if curr_plane > PLANES_PER_DIE:
				curr_plane = 0
				curr_die = curr_die + 1
				if curr_die > DIES_PER_PACKAGE:
					curr_die = 0
					curr_pack = curr_pack + 1
					if curr_pack > NUM_PACKAGES:
						curr_pack = 0
			

	print 'Script generated successfully'
	print 'RAW Hazards', RAW_haz
	print 'Delayed Writes', delayed_writes
	print 'Placed Writes', placed_writes

#========================================================================================================
# Operation Distribution Analysis Mode 1
#========================================================================================================
elif mode == "Analysis":
	EPOCH = 0;
	EPOCH_SIZE = 200000000;

	# with the perfect scheduling version of plane state we do need to keep track of the planes
	plane_states = [[[[[] for x in range(1000)] for i in range(100)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	
	# counters
	write_counter = 0
	read_counters = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	read_counter = 0
	
	# get all the plane log data
	# and pre-parse it into per plane queues
	previous_state = []
	previous_read = []

	reset_value = 0
	starting = 1;
	while(1):
		state = plane_log.readline()
		# if the state is blank we've reached the end of the file
		if state == '':
			break
	
		if state == 'Plane State Log \n':
			#do nothing for now
			print 'starting plane state parsing'
			continue
 
		
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]	
		if starting == 1:
			starting = 0
			reset_value = state_cycle-1
			
		EPOCH = (state_cycle-reset_value) % EPOCH_SIZE	
		print len(plane_states)		
		print EPOCH
		plane_states[EPOCH][package][die][plane].append([state_cycle, state_address, op])
		plane_data.append(state)

	for x in range(10000):		
		for i in range(NUM_PACKAGES):
			for j in range(DIES_PER_PACKAGE):
				for k in range(PLANES_PER_DIE):
					print 'reads for package', i, 'die', j, 'plane', k, 'is', len(plane_states[x][i][j][k])
	
else:
	print 'invalid mode selection, please enter either Read or Write'
	
output_file.close()
write_log.close()
plane_log.close()
		
		
