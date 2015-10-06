import sys

NUM_STREAMS = 10
STREAM_LENGTH = 100
STREAM_STEP = 4096
START = 512*1024*1024
STREAM_SEPARATION = 1024*1024
CYCLE_STEP = 100
RW_FLAG = 0

cycle = 0
for j in range(STREAM_LENGTH):
	for i in range(NUM_STREAMS):
		cur_addr = i*STREAM_SEPARATION + j*STREAM_STEP + START
		print '%d\t\t%d\t\t%d'%(cycle,RW_FLAG,cur_addr)

		cycle += CYCLE_STEP

