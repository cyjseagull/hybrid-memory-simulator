import sys
import pprint
import yaml

pp = pprint.PrettyPrinter()

import hybridsim

TOTAL_PAGES = 8388608
PAGE_SIZE = 4096
ADDRESS_SPACE_SIZE = TOTAL_PAGES * PAGE_SIZE
THREAD_PENDING_MAX = 8

DEBUG_SCHEDULER_PREFETCHER=False
FAKE_PREFETCHES=True

class SchedulerPrefetcher(object):
	def __init__(self, mt_tbs):
		self.mt_tbs = mt_tbs
		self.thread_pages = {}
		self.old_thread_pages = {}
		self.next_threads = None

		self.halfway_cycles = self.mt_tbs.quantum_cycles / 2

		if DEBUG_SCHEDULER_PREFETCHER:
			self.debug = open('sched_prefetch_debug.log', 'w')

	def done(self):
		outFile = open('scheduler_prefetcher.log', 'w')
		outFile.write(pp.pformat(self.old_thread_pages))
		outFile.close()

	def new_quantum(self, last_threads, next_threads):
		self.next_threads = next_threads

		if self.mt_tbs.quantum_num > 0:
			for thread_id in last_threads:
				# Save the old thread pages.
				if thread_id not in self.old_thread_pages:
					self.old_thread_pages[thread_id] = []
				self.old_thread_pages[thread_id].append(self.thread_pages[thread_id])
				
				# Reset the thread pages.
				self.thread_pages[thread_id] = {}

		if DEBUG_SCHEDULER_PREFETCHER:
			print >> self.debug, 'quantum:',self.mt_tbs.quantum_num
			print >> self.debug, 'last_threads:',last_threads
			print >> self.debug, 'next_threads:',next_threads
			print >> self.debug, self.old_thread_pages
			print >> self.debug
			self.debug.flush()
			


	def update(self):
		# Issue prefetches when halfway_cycles is reached.
		# TODO: Combine pages into ranges.
		# TODO: Use the access counts for each page to prioritize what pages are sent.
		if (self.mt_tbs.quantum_cycles_left == self.halfway_cycles) and (self.mt_tbs.quantum_num > 0):
			print 'Issuing prefetches for threads',self.next_threads
			prefetch_count = 0
			for thread_id in self.next_threads:
				if thread_id in self.old_thread_pages:
					last_thread_pages = self.old_thread_pages[thread_id][-1]
					page_list = [page_num * PAGE_SIZE for page_num in last_thread_pages.keys()]
					for page in page_list:
						if FAKE_PREFETCHES:
							self.mt_tbs.mem.mmio(4, page)
						else:
							self.mt_tbs.mem.mmio(3, page)
						prefetch_count += 1
			print 'Issued %d prefetches.'%(prefetch_count)

					

	def addTransaction(self, thread_id, isWrite, addr):
		# Compute page number.
		page_num = addr / PAGE_SIZE

		# Save page in thread pages set.
		if thread_id not in self.thread_pages:
			self.thread_pages[thread_id] = {}
		if page_num not in self.thread_pages[thread_id]:
			self.thread_pages[thread_id][page_num] = 0
		self.thread_pages[thread_id][page_num] += 1


class TraceThread(object):
	def __init__(self, thread_id, tracefile, base_address, parent):
		self.thread_id = thread_id
		self.tracefile = tracefile
		self.input_file = open(tracefile,'r')
		self.base_address = base_address
		self.parent = parent

		self.complete = 0
		self.pending = 0
		self.trace_cycles = 0 # Cycles in which we made progress in the trace file.
		self.throttle_count = 0 # Number of times we stalled during the trace execution.
		self.throttle_cycles = 0 # Number of cycles stalled during trace execution.
		self.final_cycles = 0 # Cycles passed after trace was done being generated, but while pending transactions were still outstanding.
		self.done_cycles = 0 # Cycles passed after all the trace was done and pending transactions completed.

		self.trace_done = False

		self.trans_cycle = 0
		self.trans_write = False
		self.trans_addr = 0

		self.get_next_trans()

	def update(self):
		if self.trace_done:
			if self.pending > 0:
				self.final_cycles += 1
			else:
				self.done_cycles += 1
			return

		if self.pending >= THREAD_PENDING_MAX:
			self.throttle_cycles += 1
			return

		# Called each time a clock cycle runs with this trace active.
		# This is NOT called when the trace is being stalled.
		self.trace_cycles += 1

		if self.trace_cycles >= self.trans_cycle:
			self.parent.addTransaction(self.thread_id, self.trans_write, self.trans_addr)
			self.pending += 1
			self.get_next_trans()


	def get_next_trans(self):
		# Set default values for the return tuple.

		if self.trace_done:
			return

		line = 'dummy value'
		while line:
			line = self.input_file.readline()
			tmp_line = line.strip().split('#')[0]
			if tmp_line == '':
				continue

			split_line = tmp_line.split()
			if len(split_line) != 3:
				print >> sys.stderr, 'ERROR: Parsing trace failed on line:'
				print >> sys.stderr, line
				print >> sys.stderr, 'There should be exactly three numbers per line'
				print >> sys.stderr, 'There are', len(split_line)
				sys.exit(1)

			line_vals = [int(i) for i in split_line]

			self.trans_cycle = int(split_line[0])
			self.trans_write = bool(int(split_line[1]) % 2)
			self.trans_addr = int(split_line[2])

			# Apply base address transformation.
			self.trans_addr = (self.trans_addr + self.base_address) % ADDRESS_SPACE_SIZE

			return

		# If we get to here, then there are no more transactions.
		self.done()

	
	def transaction_complete(self, isWrite, sysID, addr, cycle):
		self.pending -= 1
		self.complete += 1

		if self.trace_done and self.pending == 0:
			print 'thread',self.thread_id,'received its last pending transaction.'

	def done(self):
		print 'thread',self.thread_id,'is done issuing new transactions.'
		self.trace_done = True
		self.input_file.close()

	def print_summary(self):
		print 'thread',self.thread_id,'summary...'
		print 'tracefile = ',self.tracefile
		print 'completed transactions =',self.complete
		print 'trace_cycles =',self.trace_cycles
		print 'throttle_count =',self.throttle_count
		print 'throttle_cycles =',self.throttle_cycles
		print 'final_cycles =',self.final_cycles
		print 'done_cycles =',self.done_cycles
		print 'total_cycles = ', (self.trace_cycles + self.throttle_cycles + self.final_cycles + self.done_cycles)
		print



class MultiThreadedTBS(object):
	def __init__(self, config_file):
		self.complete = 0
		self.pending = 0

		self.cycles = 0

		self.done = False
		self.quantum_cycles_left = 0
		self.quantum_num = -1
		self.cur_running = None
		self.schedule_index = -1

		self.last_clock = 0
		self.CLOCK_DELAY = 1000000

		self.config_file = config_file


		try:
			configFile = open(self.config_file)
			config_data = yaml.load(configFile)
			configFile.close()

			self.cores = config_data['cores']
			self.quantum_cycles = config_data['quantum_cycles']
			self.trace_files = config_data['trace_files']
			self.base_addresses = config_data['base_addresses']
			self.schedule = config_data['schedule']
		except Exception as e:
			print 'Failed to parse the config file properly.'
			raise e

		#self.cores = 2
		##self.quantum_cycles = 2666667
		#self.quantum_cycles = 10000
		#self.trace_files = ['traces/big_0.txt', 'traces/big_512.txt', 'traces/big_0.txt', 'traces/big_512.txt']
		#self.base_addresses = [] # TODO: Implement this feature to add base address to all trace addresses (except kernel stuff).
		##self.schedule = [[0,2], [1,2], [2,3], [3,0]]

		# Verify the integrity of the schedule...
		for i in self.schedule:
			if len(i) != self.cores:
				print 'Schedule entry does not have length that matches core count %s'%(str(i))
				sys.exit(1)
			if len(i) != len(set(i)):
				print 'Schedule entry has a thread scheduled on more than one core: %s'%(str(i))
				sys.exit(1)

		if len(self.trace_files) != len(self.base_addresses):
			print 'Length of trace_files (%d) does not match length of base_addresses (%d)'%(len(self.trace_files), len(self.base_addresses))
			sys.exit(1)

		self.threads = {}
		for thread_id in range(len(self.trace_files)):
			self.threads[thread_id] = TraceThread(thread_id, self.trace_files[thread_id], self.base_addresses[thread_id], self)

		self.pending_transactions = {}

		# Set up the memory.
		self.mem = hybridsim.HybridSim(1, '')
		def read_cb(sysID, addr, cycle):
			self.transaction_complete(False, sysID, addr, cycle)
		def write_cb(sysID, addr, cycle):
			self.transaction_complete(True, sysID, addr, cycle)
		self.mem.RegisterCallbacks(read_cb, write_cb);

		# Set up the scheduler prefetcher.
		self.scheduler_prefetcher = SchedulerPrefetcher(self)


	def addTransaction(self, thread_id, isWrite, addr):
		self.mem.addTransaction(isWrite, addr)
		self.pending += 1

		trans_key = (addr, isWrite)
		if trans_key not in self.pending_transactions:
			self.pending_transactions[trans_key] = []
		self.pending_transactions[trans_key].append(thread_id)

		self.scheduler_prefetcher.addTransaction(thread_id, isWrite, addr)

		#print 'Added (%d,%d,%d)'%(thread_id, isWrite, addr)
		

	def transaction_complete(self, isWrite, sysID, addr, cycle):
		sysID = sysID.value
		addr = addr.value
		cycle = cycle.value

		#print 'Complete (%d,%d,%d, %d)'%(isWrite, sysID, addr, cycle)

		self.complete += 1
		self.pending -= 1

		if (self.complete % 10000 == 0) or (cycle - self.last_clock > self.CLOCK_DELAY):
			print 'Complete=',self.complete,'\t\tpending=',self.pending,'\t\tcycle_count=',cycle,'/',self.cycles,'\t\tQuantum=',self.quantum_num
			self.last_clock = cycle

		# Call the appropriate TraceThread object to tell it the transactcion is done.
		trans_key = (addr, isWrite)
		if trans_key not in self.pending_transactions:
			print 'Error: (address: %d, isWrite: %d) not in pending transactions during transaction_complete() callback!'%trans_key
			sys.exit(1)
		thread_id = self.pending_transactions[trans_key].pop(0)
		if len(self.pending_transactions[trans_key]) == 0:
			del self.pending_transactions[trans_key]

		self.threads[thread_id].transaction_complete(isWrite, sysID, addr, cycle)

		#print 'Complete thread = %d'%(thread_id)

	def clean_schedule(self):
		# This method removes done threads from the schedule if there is something else that can run.

		done_threads = []
		for thread_id in self.threads:
			if self.threads[thread_id].trace_done:
				done_threads.append(thread_id)
		done_threads.sort()

		print 'done_threads =',done_threads

		for quantum in range(len(self.schedule)):
			for i in range(len(self.schedule[quantum])):
				thread_id = self.schedule[quantum][i]
				if thread_id in done_threads:
					for new_thread_id in self.threads:
						if (new_thread_id not in self.schedule[quantum]) and (new_thread_id not in done_threads):
							self.schedule[quantum][i] = new_thread_id
							print 'Replacing thread %d with thread %d in quantum %d'%(thread_id, new_thread_id, quantum)
							break


	def new_quantum(self):
		# Determine if the simulation is done.
		tmp_done = True
		for thread_id in self.threads:
			if not self.threads[thread_id].trace_done:
				tmp_done = False # Run another quantum if any thread still has work to do.
		if tmp_done and self.pending != 0:
			print 'All threads are done, but there are still pending transactions. Running another quantum.'
			tmp_done = False # Run another quantum if there are any pending transactions.
		self.done = tmp_done
		if self.done:
			print 'Simulation is done! Here is a summary of what just happened...'
			print 'Last quantum =',self.quantum_num
			print 'Completed transactions =',self.complete
			print
			for thread_id in self.threads:
				self.threads[thread_id].print_summary()
			self.mem.printLogfile()

			self.scheduler_prefetcher.done()

			return 

		self.clean_schedule()

		self.quantum_cycles_left = self.quantum_cycles
		self.quantum_num += 1
		self.schedule_index = self.quantum_num % len(self.schedule)
		last_threads = self.cur_running
		self.cur_running = list(self.schedule[self.schedule_index])

		print 'Starting quantum %d at cycle count %d. completed=%d cur_running=%s'%(self.quantum_num, self.cycles, self.complete, str(self.cur_running))

		# TODO: Pick another thread to run if any of the current threads are done.

		next_index = (self.schedule_index + 1) % len(self.schedule)
		next_threads = self.schedule[next_index]
		self.scheduler_prefetcher.new_quantum(last_threads, next_threads)


	def run(self):
		print 'Initialization done. Starting MT-TBS run...'
		while not self.done:
			# Handle quantum switches.
			if self.quantum_cycles_left == 0:
				self.new_quantum()
				if self.done:
					return

			# Update all running threads.
			for thread_id in self.cur_running:
				self.threads[thread_id].update()

			# Update the HybridSim instance.
			self.mem.update()

			# Update the scheduler prefetcher.
			self.scheduler_prefetcher.update()

			# Update the cycle counters.
			self.cycles += 1
			self.quantum_cycles_left -= 1

			
		


class HybridSimTBS(object):
	def __init__(self):
		self.MAX_PENDING=36
		self.MIN_PENDING=36
		self.complete = 0
		self.pending = 0
		self.throttle_count = 0
		self.throttle_cycles = 0
		self.final_cycles = 0

		self.trace_cycles = 0

		self.last_clock = 0
		self.CLOCK_DELAY = 1000000

	def transaction_complete(self, isWrite, sysID, addr, cycle):
		sysID = sysID.value
		addr = addr.value
		cycle = cycle.value

		self.complete += 1
		self.pending -= 1

		if (self.complete % 10000 == 0) or (cycle - self.last_clock > self.CLOCK_DELAY):
			print 'Complete=',self.complete,'\t\tpending=',self.pending,'\t\tcycle_count=',cycle,'\t\tthrottle_count=',self.throttle_count
			self.last_clock = cycle

	def run_trace(self, tracefile):
		mem = hybridsim.HybridSim(1, '')
		
		def read_cb(sysID, addr, cycle):
			self.transaction_complete(False, sysID, addr, cycle)
		def write_cb(sysID, addr, cycle):
			self.transaction_complete(True, sysID, addr, cycle)

		mem.RegisterCallbacks(read_cb, write_cb);

		inFile = open(tracefile, 'r')

		line = 'dummy value'

		while line:
			line = inFile.readline()
			tmp_line = line.strip().split('#')[0]
			if tmp_line == '':
				continue

			split_line = tmp_line.split()
			if len(split_line) != 3:
				print >> sys.stderr, 'ERROR: Parsing trace failed on line:'
				print >> sys.stderr, line
				print >> sys.stderr, 'There should be exactly three numbers per line'
				print >> sys.stderr, 'There are', len(split_line)
				sys.exit(1)

			line_vals = [int(i) for i in split_line]

			trans_cycle = int(split_line[0])
			write = bool(int(split_line[1]) % 2)
			addr = int(split_line[2])

			while (self.trace_cycles < trans_cycle):
				mem.update()
				self.trace_cycles += 1

			mem.addTransaction(write, addr)
			self.pending += 1

			if self.pending >= self.MAX_PENDING:
				self.throttle_count += 1
				while self.pending > self.MIN_PENDING:
					mem.update()
					self.throttle_cycles += 1


		inFile.close()

		while self.pending > 0:
			mem.update()
			self.final_cycles += 1

		# This is a hack for the moment to ensure that a final write completes.
		# In the future, we need two callbacks to fix this.
		# This is not counted towards the cycle counts for the run though.
		for i in range(1000000):
			mem.update()

		# TODO: finish this up

		print 'trace_cycles =',self.trace_cycles
		print 'throttle_count =',self.throttle_count
		print 'throttle_cycles =',self.throttle_cycles
		print 'final_cycles =',self.final_cycles
		print 'total_cycles = trace_cycles + throttle_cycles + final_cycles =', (self.trace_cycles + self.throttle_cycles + self.final_cycles)

		mem.printLogfile()

def main():
	if len(sys.argv) > 1:
		yaml_file = sys.argv[1]
	else:
		yaml_file = 'ini/scheduler_prefetcher.yaml'
	hs_tbs = MultiThreadedTBS(yaml_file)
	hs_tbs.run()


if __name__ == '__main__':
	main()



