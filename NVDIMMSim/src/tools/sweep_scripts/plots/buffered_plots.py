import math 

import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt


mix = ['mix'+str(i) for i in [1,2,3,4]]
chan = [1,2,4,8,16,32]
speed = [1,2,4]

metrics = ['Cycles Simulated', 'Accesses', 'Reads completed', 'Writes completed', 'Erases completed', 'GC Reads completed', 'GC Writes completed', 'Number of Unmapped Accesses', 'Number of Mapped Accesses', 'Number of Unmapped Reads', 'Number of Mapped Reads', 'Number of Unmapped Writes', 'Number of Mapped Writes', 'Average Read Latency', 'Average Write Latency', 'Average Erase Latency', 'Average Garbage Collector initiated Read Latency', 'Average Garbage Collector initiated Write Latency', 'Average Queue Latency', 'Total Throughput', 'Read Throughput', 'Write Throughput', 'Length of Ftl Queue', 'Length of GC Queue']

def gen_metrics():
	for c in chan:
		metrics.append(['Length of Controller Queue for Package '+str(c), 'Accumulated Idle Energy for Package '+str(c), 'Accumulated Access Energy for Package '+str(c), 'Accumulated Erase Energy for Package '+str(c), 'Total Energy for Package '+str(c), 'Average Idle Power for Package '+str(c), 'Average Access Power for Package '+str(c), 'Average Erase Power for Package '+str(c), 'Average Power for Package '+str(c)])

def parse_nvdimmsim_log(logfile):
	inFile = open(logfile, 'r')
	lines = inFile.readlines()
	inFile.close()
	[i.strip() for i in lines]
	
	result = {}

	result['Cycles Simulated'] = int(lines[5].split(':')[1])
	result['Accesses'] = float(lines[6].split(':')[1])
	result['Reads completed'] = float(lines[7].split(':')[1])
	result['Writes completed'] = float(lines[8].split(':')[1])
	result['Erases completed'] = float(lines[9].split(':')[1])
	result['GC Reads completed'] = float(lines[10].split(':')[1])
	result['GC Writes completed'] = float(lines[11].split(':')[1])
	result['Number of Unmapped Accesses'] = float(lines[12].split(':')[1])
	result['Number of Mapped Accesses'] = float(lines[13].split(':')[1])
	result['Number of Unmapped Reads'] = float(lines[14].split(':')[1])
	result['Number of Mapped Reads'] = float(lines[15].split(':')[1])
	result['Number of Unmapped Writes'] = float(lines[16].split(':')[1])
	result['Number of Mapped Writes'] = float(lines[17].split(':')[1])
	result['Average Read Latency'] = float(lines[24].split(':')[1].split('cycles')[0])
	result['Average Write Latency'] = float(lines[25].split(':')[1].split('cycles')[0])
	result['Average Erase Latency'] = float(lines[26].split(':')[1].split('cycles')[0])
	result['Average Garbage Collector initiated Read Latency'] = float(lines[27].split(':')[1].split('cycles')[0])
	result['Average Garbage Collector initiated Write Latency'] = float(lines[28].split(':')[1].split('cycles')[0])
	result['Average Queue Latency'] = float(lines[29].split(':')[1].split('cycles')[0])
	result['Total Throughput'] = float(lines[30].split(':')[1].split()[0])
	result['Read Throughput'] = float(lines[31].split(':')[1].split()[0])
	result['Write Throughput'] = float(lines[32].split(':')[1].split()[0])
	result['Length of Ftl Queue'] = float(lines[36].split(':')[1])
	result['Length of GC Queue'] = float(lines[37].split(':')[1])
			       
        queues = 0
	count = 0
	for c in chan:
		result['Length of Controller Queue for Package '+str(c)] = float(lines[38].split(':')[1])
		queues = queues + 1
	for h in chan:
		result['Accumulated Idle Energy for Package '+str(h)] = float(lines[43+queues+count].split(':')[1].split()[0])
		result['Accumulated Access Energy for Package '+str(h)] = float(lines[44+queues+count].split(':')[1].split()[0])
		result['Accumulated Erase Energy for Package '+str(h)] = float(lines[45+queues+count].split(':')[1].split()[0])
		result['Total Energy for Package '+str(h)] = float(lines[46+queues+count].split(':')[1].split()[0])
		result['Average Idle Power for Package '+str(h)] = float(lines[47+queues+count].split(':')[1].split()[0])
		result['Average Access Power for Package '+str(h)] = float(lines[48+queues+count].split(':')[1].split()[0])
		result['Average Erase Power for Package '+str(h)] = float(lines[49+queues+count].split(':')[1].split()[0])
		result['Average Power for Package '+str(h)] = float(lines[50+queues+count].split(':')[1].split()[0])
		count = count + 2
		 
	return result

def parse_all_results():
	results = {}
	for i in mix:
		results[i] = {}
		for j in chan:
			results[i][j] = {}
			for s in speed:
				logfile = '../'+i+'/'+str(j)+'/'+str(s)+'/nvdimm_logs/NVDIMM.log'

				results[i][j][s] = parse_nvdimmsim_log(logfile)
	return results

def plot(param, data):
	plt.clf()

	print param

	cnt = 1
	for m in mix:
		# Collect the data I care about
		x = [math.log(i,2) for i in chan]
		y1 = [data[m][i][s][param] for i in chan for s in speed]
		y2 = [data[m][i][s][param] for i in chan for s in speed]

		all_y = y1+y2
		pad_y = int((max(all_y)-min(all_y))*0.1)
		

		plt.subplot(220+cnt)
		l = plt.plot(x, y1, 'ko--', x, y2, 'bo--')
		plt.xlabel('log2(Channels)')
		plt.ylabel(param)
		plt.title(m)
		if param in [metrics[1], metrics[8], metrics[9]]:
			plt.axis([-1, 9, min(all_y), max(all_y)])
		else:
			plt.axis([-1, 9, min(all_y)-10, max(all_y)+10])
		
		plt.grid(True)
		plt.subplots_adjust(hspace=0.5, wspace=0.5)

		cnt += 1


	plt.savefig(param+'.png')

gen_metrics()
data = parse_all_results()
#print data['mix2']
#print mix

for i in metrics:
	plot(i, data)
