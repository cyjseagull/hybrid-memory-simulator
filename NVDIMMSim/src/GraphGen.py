import matplotlib

def parse_file(logfile):
	section = '================================================='

	inFile = open(logfile, 'r')
	lines = inFile.readlines()
	inFile.close()
	lines = [line.strip() for line in lines]

	section_list = []
	for i in range(len(lines)):
		if lines[i] == section:
			section_list.append(i)

	sections = {}
	sections['total'] = lines[5:section_list[0]-2]	
	sections['epoch'] = lines[section_list[0]+1:]	

	return sections


def parse_total(total_lines):
	sections = ['access', 'latency', 'queue', 'write', 'power']
	total_dict = {}
	for j in sections:
		total_dict[j] = {}
	cur_section = 0
	starting = 0

	for i in total_lines:
		if i == '':
			continue
		elif i == 'Access Data:': 
			continue
		elif i == 'Throughput and Latency Data:':
			continue
		elif i == 'Queue Length Data:':
			continue
		elif i == 'Write Frequency Data:':
			continue
		elif i == 'Power Data:':
			continue
		elif i == '===========================':
			continue
		elif i == '========================':
			if starting == 0:
				starting = 1
				continue
			else:			
				cur_section += 1
				print cur_section
				continue
		else:
			tmp = i.split(':')
			key = tmp[0].strip()
			val = tmp[1].strip()			
			if key.endswith('Latency'):
				latency_vals = [v.strip(') ') for v in val.split('(')]
				total_dict[sections[cur_section]][key+' cycles'] = float(latency_vals[0].split(' ')[0])
				total_dict[sections[cur_section]][key+' ns'] = float(latency_vals[1].split(' ')[0])
			elif key.endswith('Throughput'):
				total_dict[sections[cur_section]][key+' KB/sec'] = float(latency_vals[0].split(' ')[0])
			elif cur_section == 3:
				total_dict[sections[cur_section]][key+' writes'] = float(latency_vals[0].split(' ')[0])
			elif key.endswith('Energy'):
				total_dict[sections[cur_section]][key+' mJ'] = float(latency_vals[0].split(' ')[0])
			elif key.endswith('Power'):
				total_dict[sections[cur_section]][key+' mW'] = float(latency_vals[0].split(' ')[0])
			elif key.endswith('Epoch'):
				total_dict[sections[cur_section]]['Epoch'] = val
			else:
				total_dict[sections[cur_section]][key] = val
				

	print total_dict[sections[1]]
	return total_dict


def parse_epoch(epoch_lines):
	epoch_section = '-------------------------------------------------'
	
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
		epoch_list.append(parse_total(i[1:-2]))

	return epoch_list


sections = parse_file('NVDIMM.log')
total_dict = parse_total(sections['total'])
epoch_list = parse_epoch(sections['epoch'])

#print epoch_list[4]

print total_dict
