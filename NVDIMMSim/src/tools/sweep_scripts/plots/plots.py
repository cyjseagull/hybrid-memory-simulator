import math 

import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt


mix = ['mix'+str(i) for i in [1,2,3,4]]
chan = [1,2,4,8,16,32,64,128,256]
config = ['prefetch', 'noprefetch']

metrics = ['cycles', 'miss rate', 'throughput', 'average queue length', 'average latency', 'average queue latency', 'average miss latency', 
		'average hit latency', 'flash idle percentage', 'dram idle percentage']

def parse_hybridsim_log(logfile):
	inFile = open(logfile, 'r')
	lines = inFile.readlines()
	inFile.close()
	[i.strip() for i in lines]
	
	result = {}

	result['cycles'] = int(lines[1].split(':')[1])
	result['miss rate'] = float(lines[6].split(':')[1])
	result['throughput'] = float(lines[11].split(':')[1].split()[0])
	result['average queue length'] = float(lines[16].split(':')[1])
	result['average latency'] = float(lines[7].split(':')[1].split('cycles')[0])
	result['average queue latency'] = float(lines[8].split(':')[1].split('cycles')[0])
	result['average miss latency'] = float(lines[9].split(':')[1].split('cycles')[0])
	result['average hit latency'] = float(lines[10].split(':')[1].split('cycles')[0])
	result['flash idle percentage'] = float(lines[20].split(':')[1])
	result['dram idle percentage'] = float(lines[22].split(':')[1])

	return result

def parse_all_results():
	results = {}
	for i in mix:
		results[i] = {}
		for j in chan:
			results[i][j] = {}
			for k in config:
				logfile = '../'+i+'/'+str(j)+'/'+k+'/HybridSim/hybridsim.log'

				results[i][j][k] = parse_hybridsim_log(logfile)
	return results

def plot(param, data):
	plt.clf()

	print param

	cnt = 1
	for m in mix:
		# Collect the data I care about
		x = [math.log(i,2) for i in chan]
		y1 = [data[m][i]['noprefetch'][param] for i in chan]
		y2 = [data[m][i]['prefetch'][param] for i in chan]

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


data = parse_all_results()
#print data['mix2']
#print mix

for i in metrics:
	plot(i, data)
