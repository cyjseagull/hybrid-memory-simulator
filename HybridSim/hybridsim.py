import ctypes
from ctypes import byref
from ctypes import c_ulonglong

lib = ctypes.cdll.LoadLibrary('./libhybridsim.so')

class HybridSim(object):
	def __init__(self, sys_id, ini):
		self.hs = lib.HybridSim_C_getMemorySystemInstance(sys_id, ini)
		self.read_cb = None
		self.write_cb = None

	def RegisterCallbacks(self, read_cb, write_cb):
		self.read_cb = read_cb
		self.write_cb = write_cb

	def addTransaction(self, isWrite, addr):
		return lib.HybridSim_C_addTransaction(self.hs, isWrite, c_ulonglong(addr))

	def WillAcceptTransaction(self):
		return lib.HybridSim_C_WillAcceptTransaction(self.hs)

	def update(self):
		lib.HybridSim_C_update(self.hs)

		self.handle_callbacks()

	def handle_callbacks(self):
		sysID = ctypes.c_uint()
		addr = ctypes.c_ulonglong()
		cycle = ctypes.c_ulonglong()
		isWrite = ctypes.c_bool()
		
		while lib.HybridSim_C_PollCompletion(self.hs, byref(sysID), byref(addr), byref(cycle), byref(isWrite)):
			if isWrite:
				if self.write_cb:
					self.write_cb(sysID, addr, cycle)
			else:
				if self.read_cb:
					self.read_cb(sysID, addr, cycle)

	def mmio(self, operation, address):
		lib.HybridSim_C_mmio(self.hs, operation, address)

	def syncAll(self):
		lib.HybridSim_C_syncAll(self.hs)

	def reportPower(self):
		lib.HybridSim_C_reportPower(self.hs)

	def printLogfile(self):
		lib.HybridSim_C_printLogfile(self.hs)

def read_cb(sysID, addr, cycle):
	print 'cycle %d: read callback from sysID %d for addr = %d'%(cycle.value, sysID.value, addr.value)
def write_cb(sysID, addr, cycle):
	print 'cycle %d: write callback from sysID %d for addr = %d'%(cycle.value, sysID.value, addr.value)
		
def main():
	hs = HybridSim(0, '')
	hs.RegisterCallbacks(read_cb, write_cb)


	hs.addTransaction(1, 0)
	hs.addTransaction(1, 8)
	hs.addTransaction(1, 16)
	hs.addTransaction(1, 24)

	for i in range(100000):
		hs.update()

	hs.WillAcceptTransaction()
	hs.reportPower()
	hs.mmio(0,0)
	hs.syncAll()

	hs.printLogfile()


if __name__ == '__main__':
	main()

