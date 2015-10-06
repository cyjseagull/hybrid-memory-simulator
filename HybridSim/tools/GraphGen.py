import numpy as np

import matplotlib
matplotlib.use('Agg') # To allow for running without X.
import matplotlib.pyplot as plt

import pprint

FIGURE_CNT = 1

def parse_file(logfile):
	section = '================================================================================'

	inFile = open(logfile, 'r')
	lines = inFile.readlines()
	inFile.close()
	lines = [line.strip() for line in lines]

	section_list = []
	for i in range(len(lines)):
		if lines[i] == section:
			section_list.append(i)
		
	sections = {}
	sections['total'] = lines[0:section_list[0]-2]	
	sections['epoch'] = lines[section_list[0]+5:section_list[1]]	
	sections['miss'] = lines[section_list[1]+3:section_list[2]-2]	
	sections['access'] = lines[section_list[2]+3:section_list[3]-2]
	sections['latency'] = lines[section_list[3]+4:section_list[4]-2]
	sections['sets'] = lines[section_list[4]+4:]

	return sections


def parse_total(total_lines):
	sections = ['total', 'read', 'write']
	total_dict = {}
	for j in sections:
		total_dict[j] = {}
	cur_section = 0

	for i in total_lines:
		if i == '':
			cur_section += 1 
			continue
		tmp = i.split(':')
		key = tmp[0].strip()
		val = tmp[1].strip()
		if key.endswith('latency'):
			latency_vals = [v.strip(') ') for v in val.split('(')]
			total_dict[sections[cur_section]][key+' cycles'] = float(latency_vals[0].split(' ')[0])
			total_dict[sections[cur_section]][key+' us'] = float(latency_vals[1].split(' ')[0])
		else:
			try:
				# Try to parse it as an int.
				total_dict[sections[cur_section]][key] = int(val.split()[0])
			except:
				# If that doesn't work, then parse it as a float.
				total_dict[sections[cur_section]][key] = float(val.split()[0])

	return total_dict


def parse_epoch(epoch_lines):
	epoch_section = '---------------------------------------------------'
	
	epoch_lines_list = []
	cur_section = []
	for i in epoch_lines:
		if i == epoch_section:
			epoch_lines_list.append(cur_section)
			cur_section = []
			continue
		cur_section.append(i)

	epoch_list = []
	for i in epoch_lines_list:
		epoch_list.append(parse_total(i[:-2]))

	return epoch_list



def parse_misses(miss_lines):
	miss_list = []
	for i in miss_lines:
		cur_dict = {}
		tmp = i.split(':')
		cur_dict['address'] = int(tmp[0], 16)
		data = tmp[1].split(';')
		for j in data:
			if j == '':
				continue
			key, val = j.split('=')
			key = key.strip()
			val = val.strip()
			if val.startswith('0x'):
				cur_dict[key] = int(val, 16)
			else:
				cur_dict[key] = int(val)
		miss_list.append(cur_dict)
	return miss_list


def parse_accesses(access_lines):
	access_dict = {} 
	for i in access_lines:
		tmp = i.split(':')
		key = int(tmp[0], 16)
		val = int(tmp[1])
		access_dict[key] = val
	return access_dict

def parse_latency(latency_lines):
	latency_dict = {}

	for i in latency_lines:
		if i != '':
			tmp = i.split(':')		
			key = tmp[0].strip()
			if key.isdigit():
				key = int(key)
			val = int(tmp[1].strip())
			latency_dict[key] = val

	return latency_dict

def parse_sets(set_lines):
	set_dict = {}

	for i in set_lines:
		if i != '':
			tmp = i.split(':')		
			key = int(tmp[0].strip())
			val = int(tmp[1].strip())
			set_dict[key] = val

	return set_dict

def parse_log(filename):
	sections = parse_file(filename)

	log_data = {}
	log_data['total'] = parse_total(sections['total'])
	log_data['epoch'] = parse_epoch(sections['epoch'])
	log_data['miss'] = parse_misses(sections['miss'])
	log_data['access'] = parse_accesses(sections['access'])
	log_data['latency'] = parse_latency(sections['latency'])
	log_data['sets'] = parse_sets(sections['sets'])

	return log_data

def parse_logs(file_dict):
	log_dict = {}
	for i in file_dict:
		log_dict[i] = parse_log(file_dict[i])
	return log_dict
	
def pretty_print(log_data, d=6):
	pp = pprint.PrettyPrinter(depth=d)
	pp.pprint(log_data)

def new_figure():
	global FIGURE_CNT
	plt.figure(FIGURE_CNT)
	FIGURE_CNT += 1
	

def plot_latency_histogram(log_data, output_file):
	# Get the latency data.
	latency_dict = log_data['latency']

	# Number of bins.
	N = latency_dict['HISTOGRAM_MAX'] / latency_dict['HISTOGRAM_BIN'] + 1

	# Get the latency histogram values.
	vals = []
	for i in range(N):
		vals.append(latency_dict[i*latency_dict['HISTOGRAM_BIN']])

	# Make the plot.
	ind = np.arange(N)    # the x locations 
	new_figure()
	p1 = plt.bar(ind, vals)

	# Label it.
	plt.ylabel('Accesses')
	plt.xlabel('Latency (x100)')
	plt.title('Histogram of HybridSim Latencies')

	# Save to file.
	plt.savefig(output_file)

def plot_misses(log_data, output_file):
	miss_list = log_data['miss']
	x_vals = []
	y_vals = []
	for i in miss_list:
		x_vals.append(i['address'])
		y_vals.append(i['missed'])

	new_figure()
	p1 = plt.scatter(x_vals, y_vals)

	plt.ylabel('Address')
	plt.xlabel('Cycle')
	plt.title('HybridSim Cache Misses')

	plt.savefig(output_file)

def plot_epochs(log_data, output_file, a, b):
	epoch_list = log_data['epoch'] 
	
	x_vals = []
	y_vals = []
	for i in range(len(epoch_list)):
		x_vals.append(i)
		y_vals.append(epoch_list[i][a][b])

	new_figure()
	p1 = plt.scatter(x_vals, y_vals)

	y_label = a+' '+b
	plt.ylabel(y_label)
	plt.xlabel('Epoch')
	plt.title('HybridSim '+y_label+' per epoch')

	plt.savefig(output_file)


log_dict = parse_logs({1: 'hybridsim.log', 2: 'hybridsim2.log'})
print log_dict[2]
#log_data = parse_log('hybridsim.log')

#pretty_print(log_data['miss'], 2)
#pretty_print(log_data, 6)

#plot_latency_histogram(log_data, 'plots/latency.png')
#plot_misses(log_data, 'plots/misses.png')
#plot_epochs(log_data, 'plots/accesses_per_epoch.png', 'total', 'total accesses')
