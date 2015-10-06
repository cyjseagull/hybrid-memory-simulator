import os
import sys

cleanup = False
mstats = ''

# Read the ini file in and eval it.
# Let's hope the ini author didn't do anything malicious. :)
if len(sys.argv) < 2:
	print 'Need definitions file'
	sys.exit(1)
server = sys.argv[1]
exec('from '+server+' import *')

# Edit this stuff.
#latency_base = ['InitialComparison']
#latency_runs = ['1Ferret32_4', '1Fluid32_1', '1Fluid32_4', '1Fluid32_5', '1Fluid64', '1Fluid64_2', '1Mix64_1', '1Mix64_2', '1Mix64_3', '1Mix64_4', '1Mix64_5']
#latency_runs = ['1Ferret32_4']

#prefetching_base = ['PrefetchingSweep']
#prefetching_runs = ['1Mix64_4']

#channel_base = ['ChannelSweep']
#channel_runs = ['1MineSLC_1', '1MineSLC_2', '1MineSLC_3', '1MineSLC_4', '1MineSLC_5', '1MineSLC_6']
#channel_runs = ['1MineSLC_1']

#buffering_base = ['BufferingSweep']
#buffering_runs = ['1Mine32_1', '1Mine32_2', '1Mine32_3', '1Mine32_4', '1Mine32_5']


####################################################3
# Do not edit below this point.


if cleanup:
	SSD = 'SSD'
	Hybrid = 'Hybrid'
else:
	SSD = 'SSD/marss.dramsim'
	Hybrid = 'Hybrid/marss.hybridsim'
	
read_latencies = ['33112', '16556', '8278', '4139', '2069', '40', '33']
prefetching_window = ['0','4','8','16']
channels = ['4','8','16','32']
dies = ['2','4']
channel_count = ['1','2','4','8','16']

latency_list = [latency_base, latency_runs, read_latencies, [SSD, Hybrid]]
if cleanup:
	prefetching_list = [prefetching_base,prefetching_runs,read_latencies,['Hybrid'],prefetching_window]
else:
	prefetching_list = [prefetching_base,prefetching_runs,read_latencies,['Hybrid'],prefetching_window,['marss.hybridsim']]
channel_list = [channel_base,channel_runs,channels,dies,[SSD,Hybrid]]
buffering_list = [buffering_base,buffering_runs,channel_count,[Hybrid]]



if cleanup:
	instr_cmd = 'python '+mstats+' -y --flatten -n base_machine::ooo_.\*::thread0::commit::insns -t user --sum %s.yml'
	cycle_cmd = 'python '+mstats+' -y --flatten -n base_machine::ooo_.\*::cycles -t total --sum %s.yml'
else:
	instr_cmd = 'python util/mstats.py -y --flatten -n base_machine::ooo_.\*::thread0::commit::insns -t user --sum %s.yml'
	cycle_cmd = 'python util/mstats.py -y --flatten -n base_machine::ooo_.\*::cycles -t total --sum %s.yml'
	
#grep_cmd = 'grep "Stopped after" %s.log'

def process_dir(curpath, inputlist):
	if inputlist == []:
		return [curpath]
	else:
		paths = []
		for i in inputlist[0]:
			paths.extend(process_dir(curpath+'/'+i, inputlist[1:]))
		return paths

latency_paths = process_dir('.', latency_list)
prefetching_paths = process_dir('.', prefetching_list)
channel_paths = process_dir('.', channel_list)
buffering_paths = process_dir('.', buffering_list)

all_paths = [latency_paths, prefetching_paths, channel_paths, buffering_paths]
#all_paths = [latency_paths]

latency_results = {}
prefetching_results = {}
channel_results = {}
buffering_results = {}

bad_path = []
didnt_finish = []


def getfile(path):
	inFile = open(path,'r')
	return inFile.readlines()

def writefile(data, filename):
	outFile = open(filename, 'w')
	outFile.write(str(data))
	outFile.close()
	

def process_path(path):
	# Confirm the directory exists.
	if not os.path.exists(path):
		print 'Path '+path+' does not exist.'
		bad_path.append(path)
		return

	os.system('cd '+path+'; pwd;')


	try:
		# Get the user instructions
		if not os.path.exists(path+'/hybridsim.log'):
			print 'Path '+path+' did not finish!'
			user_ipc = 0.0
			didnt_finish.append(path)
		else:
			# Get the log file base.
			os.system('cd '+path+'; ls *.yml > ymlname;')
			benchmark = getfile(path+'/ymlname')[0].strip()[:-4]
			os.system('cd '+path+'; rm ymlname;')

			# Get the instruction count
			os.system('cd '+path+'; '+(instr_cmd%benchmark)+' > instrout;')
			user_str = getfile(path+'/instrout')
			user_instr = user_str[0].strip().split(None, 3)[2]
			os.system('cd '+path+'; rm instrout;')
			#print user_instr
			
			# Get the cycle count
			os.system('cd '+path+'; '+(cycle_cmd%benchmark)+' > cycleout;')
			cycle_str = getfile(path+'/cycleout')
			cycle_cnt = cycle_str[0].strip().split(None, 3)[2]
			cycle_cnt = float(cycle_cnt) / 4.0
			os.system('cd '+path+'; rm cycleout;')
			#print cycle_cnt

			# Compute the user IPC
			user_ipc = float(user_instr) / float(cycle_cnt)
			#print user_ipc
	except IndexError:
		print 'Path '+path+' caused an IndexError.'
		user_ipc = 0.0
		didnt_finish.append(path)

	# Determine what type of experiment this is
	if path in latency_paths:
		latency = path.split('/')[3]
		sim_type = path.split('/')[4]
		print benchmark,sim_type,latency,user_ipc

		# Create the dictionary entries needed for this benchmark/latency combo.
		if benchmark not in latency_results:
			latency_results[benchmark] = {}
		if latency not in latency_results[benchmark]:
			latency_results[benchmark][latency] = {}
			latency_results[benchmark][latency]['Hybrid'] = []
			latency_results[benchmark][latency]['SSD'] = []

		latency_results[benchmark][latency][sim_type].append(user_ipc)
		

	elif path in prefetching_paths:
		latency = path.split('/')[3]
		prefetch_window = path.split('/')[5]
		print benchmark,latency,prefetch_window,user_ipc

		if benchmark not in prefetching_results:
			prefetching_results[benchmark] = {}
		if latency not in prefetching_results[benchmark]:
			prefetching_results[benchmark][latency] = {}
		if prefetch_window not in prefetching_results[benchmark][latency]:
			prefetching_results[benchmark][latency][prefetch_window] = []

		prefetching_results[benchmark][latency][prefetch_window].append(user_ipc)


	elif path in channel_paths:
		channel = path.split('/')[3]
		die = path.split('/')[4]
		sim_type = path.split('/')[5]
		print benchmark,channel,die,user_ipc

		if benchmark not in channel_results:
			channel_results[benchmark] = {}
		if channel not in channel_results[benchmark]:
			channel_results[benchmark][channel] = {}
		if die not in channel_results[benchmark][channel]:
			channel_results[benchmark][channel][die] = {}
			channel_results[benchmark][channel][die]['Hybrid'] = []
			channel_results[benchmark][channel][die]['SSD'] = []

		channel_results[benchmark][channel][die][sim_type].append(user_ipc)


	elif path in buffering_paths:
		channel = path.split('/')[3]
		print benchmark,channel,user_ipc

		if benchmark not in buffering_results:
			buffering_results[benchmark] = {}
		if channel not in buffering_results[benchmark]:
			buffering_results[benchmark][channel] = {}
			buffering_results[benchmark][channel] = []

		buffering_results[benchmark][channel].append(user_ipc)


for experiment_paths in all_paths:
	for path in experiment_paths:
		process_path(path)


writefile(latency_results, 'latency_'+server+'.txt')
writefile(prefetching_results, 'prefetching_'+server+'.txt')
writefile(channel_results, 'channel_'+server+'.txt')
writefile(buffering_results, 'buffering_'+server+'.txt')

outFile = open('failed_'+server+'.txt','w')
outFile.write('Paths that did not exist:\n')
for i in bad_path:
	outFile.write(i+'\n')
outFile.write('\nPaths that did not finish:\n')
for i in didnt_finish:
	outFile.write(i+'\n')
outFile.close()

