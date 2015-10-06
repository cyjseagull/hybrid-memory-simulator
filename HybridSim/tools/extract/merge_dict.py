import sys
import pprint
pp = pprint.PrettyPrinter(indent=4)

POINTS = 3

if len(sys.argv) < 4:
	sys.stderr.write('Usage: '+sys.argv[0]+' mode inputfile1 inputfile2 ...\n')
	sys.stderr.write('Valid modes are: latency, prefetching, channel, buffering\n')
	sys.stderr.write('Need at least two input files.\n')
	sys.exit(1)

dict_list = []
dict_names = []

# Check the mode
mode = sys.argv[1]
valid_modes = ['latency', 'prefetching', 'channel', 'buffering']
if mode not in valid_modes:
	sys.stderr.write('Mode '+mode+' is invalid.\n')
	sys.stderr.write('Valid modes are: '+str(valid_modes)+'\n')
	sys.exit(1)

# Load the dictionries from file
for i in sys.argv[2:]:
	inFile = open(i, 'r')
	instr = inFile.read()
	inFile.close()
	try:
		indict = eval(instr)
	except:
		sys.stderr.write(i+' did not work with the eval() function.\n')
		sys.exit(1)
	if not type(indict) == type({}):
		sys.stderr.write(i+' did not generate a dictionary data structure.\n')
		sys.exit(1)
	dict_list.append(indict)
	dict_names.append(i)

print 'Parsed the following dictionaries:'
print dict_names


# Merge the dictionaries.
num_runs = 0
num_good = 0
master = {}

if mode == 'latency':
	for i in range(len(dict_list)):
		cur = dict_list[i]
		curname = dict_names[i]
		for benchmark in cur:
			if benchmark not in master:
				master[benchmark] = {}
			for latency in cur[benchmark]:
				sim_types = ['Hybrid', 'SSD']
				if latency not in master[benchmark]:
					master[benchmark][latency] = {}
					for sim_type in sim_types:
						master[benchmark][latency][sim_type] = []
				for sim_type in sim_types:
					if sim_type not in cur[benchmark][latency]:
						sys.stderr.write(sim_type+' not found in '+curname+'['+benchmark+']['+latency+']\n')
						sys.exit(1)
					master[benchmark][latency][sim_type].extend(cur[benchmark][latency][sim_type])
					num_runs += len(cur[benchmark][latency][sim_type])
					num_good += len([i for i in cur[benchmark][latency][sim_type] if i != 0.0])

elif mode == 'prefetching':
	for i in range(len(dict_list)):
		cur = dict_list[i]
		curname = dict_names[i]
		for benchmark in cur:
			if benchmark not in master:
				master[benchmark] = {}
			for latency in cur[benchmark]:
				if latency not in master[benchmark]:
					master[benchmark][latency] = {}
				for window in cur[benchmark][latency]:
					if window not in master[benchmark][latency]:
						master[benchmark][latency][window] = []
					master[benchmark][latency][window].extend(cur[benchmark][latency][window])
					num_runs += len(cur[benchmark][latency][window])
					num_good += len([i for i in cur[benchmark][latency][window] if i != 0.0])


elif mode == 'channel':
	for i in range(len(dict_list)):
		cur = dict_list[i]
		curname = dict_names[i]
		for benchmark in cur:
			if benchmark not in master:
				master[benchmark] = {}
			for channels in cur[benchmark]:
				if channels not in master[benchmark]:
					master[benchmark][channels] = {}
				for dies in cur[benchmark][channels]:
					sim_types = ['Hybrid', 'SSD']
					if dies not in master[benchmark][channels]:
						master[benchmark][channels][dies] = {}
						for sim_type in sim_types:
							master[benchmark][channels][dies][sim_type] = []
					for sim_type in sim_types:
						if sim_type not in cur[benchmark][channels][dies]:
							sys.stderr.write(sim_type+' not found in '+curname+'['+benchmark+']['+channels+']['+dies+']\n')
							sys.exit(1)
						master[benchmark][channels][dies][sim_type].extend(cur[benchmark][channels][dies][sim_type])
						num_runs += len(cur[benchmark][channels][dies][sim_type])
						num_good += len([i for i in cur[benchmark][channels][dies][sim_type] if i != 0.0])

elif mode == 'buffering':
	for i in range(len(dict_list)):
		cur = dict_list[i]
		curname = dict_names[i]
		for benchmark in cur:
			if benchmark not in master:
				master[benchmark] = {}
			for channels in cur[benchmark]:
				if channels not in master[benchmark]:
					master[benchmark][channels] = []
				master[benchmark][channels].extend(cur[benchmark][channels])
				num_runs += len(cur[benchmark][channels])
				num_good += len([i for i in cur[benchmark][channels] if i != 0.0])


def int_sorted(str_list):
	num_list = [int(i) for i in str_list]
	num_list.sort()
	return [str(i) for i in num_list]
def float_sorted(str_list):
	num_list = [float(i) for i in str_list]
	num_list.sort()
	return [str(i) for i in num_list]

#pp.pprint(master)
print 'Total runs:',num_runs
print 'Good runs:',num_good

table_list = []

# Generate the table format

if mode == 'latency':
	for benchmark in sorted(master.keys()):
		for latency in int_sorted(master[benchmark].keys()):
			new_entry = benchmark+'\t'+latency+'\t'
			sim_types = ['Hybrid', 'SSD']
			for sim_type in sim_types:
				count = 0
				for ipc in master[benchmark][latency][sim_type]:
					if ipc != 0.0:
						new_entry += str(ipc) +'\t'
						count += 1
					if count == POINTS:
						break
				while count < POINTS:
					new_entry += str(0.0) + '\t'
					count += 1
			table_list.append(new_entry)

elif mode == 'prefetching':
	for benchmark in master:
		for latency in int_sorted(master[benchmark].keys()):
			for window in int_sorted(master[benchmark][latency].keys()):
				new_entry = benchmark+'\t'+latency+'\t'+window+'\t'
				count = 0
				for ipc in master[benchmark][latency][window]:
					if ipc != 0.0:
						new_entry += str(ipc) +'\t'
						count += 1
					if count == POINTS:
						break
				while count < POINTS:
					new_entry += str(0.0) + '\t'
					count += 1
				table_list.append(new_entry)


elif mode == 'channel':
	for benchmark in master:
		for channels in int_sorted(master[benchmark].keys()):
			for dies in int_sorted(master[benchmark][channels].keys()):
				sim_types = ['Hybrid', 'SSD']
				new_entry = benchmark+'\t'+channels+'\t'+dies+'\t'
				for sim_type in sim_types:
					count = 0
					for ipc in master[benchmark][channels][dies][sim_type]:
						if ipc != 0.0:
							new_entry += str(ipc) +'\t'
							count += 1
						if count == POINTS:
							break
					while count < POINTS:
						new_entry += str(0.0) + '\t'
						count += 1
				table_list.append(new_entry)



elif mode == 'buffering':
	for benchmark in master:
		for channels in int_sorted(master[benchmark].keys()):
			new_entry = benchmark+'\t'+channels+'\t'
			count = 0
			for ipc in master[benchmark][channels]:
				if ipc != 0.0:
					new_entry += str(ipc) +'\t'
					count += 1
				if count == POINTS:
					break
			while count < POINTS:
				new_entry += str(0.0) + '\t'
				count += 1
			table_list.append(new_entry)

print 'table:'
for i in table_list:
	print i
