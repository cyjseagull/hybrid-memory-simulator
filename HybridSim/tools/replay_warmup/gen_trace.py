# This tool is used in the fast forwarding process to generate HybridSim format 
# traces from the GreenskySim format.
# Usage: python gen_trace.py <tracefile>

import sys
import struct

base = sys.argv[1]
num_input = int(sys.argv[2])

files = [base+str(i) for i in range(num_input)]

cycle = 0

for filename in files:
	with open(filename, 'r') as f:
		length = 1
		byte = f.read(1)
		while ord(byte) != 0:
			byte = f.read(1)
			length += 1

		(byte,) = struct.unpack('<L', f.read(4))
		length = byte - length - 4
		while length:
			(flags,) = struct.unpack('B', f.read(1))
			(threadid,) = struct.unpack('B', f.read(1))
			(size,) = struct.unpack('<H', f.read(2))
			(vaddr,) = struct.unpack('<Q', f.read(8))
			(paddr,) = struct.unpack('<Q', f.read(8))
			#print hex(flags), hex(threadid), hex(size), hex(vaddr), hex(paddr)
			length -= 20

			writebit = flags
			print str(cycle) + '\t\t' + str(writebit) + '\t\t' + str(paddr)
			cycle += 1000 # space out 1000 cycles to keep queue from jamming up.

#        if cycle > 100000:
#            break
