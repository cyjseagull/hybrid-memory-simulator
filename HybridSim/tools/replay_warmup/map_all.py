print '4096 64 2097152 2097152'

for i in range(2097152):
	cache_addr = i * 4096
	tag = i / 32768 
	print str(cache_addr) + ' 1 0 ' + str(tag) + ' 0 0'
