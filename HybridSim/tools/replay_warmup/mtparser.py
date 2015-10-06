# This is the original parser for GreenskySim traces written by Jim Greensky.
# Note: GreenskySim is our name for a modified version of QEMU that outputs
# trace data from QEMU. It outputs a raw hex format, which is then preprocessed
# into a simple ASCII format before being sent to HybridSim.
# Usage: python mtparser.py <trace>

import sys
import struct

with open(sys.argv[1], 'r') as f:
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
        print hex(flags), hex(threadid), hex(size), hex(vaddr), hex(paddr)
        length -= 20
