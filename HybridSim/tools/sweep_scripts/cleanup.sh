topdir=`pwd`


for mix in mix1 mix2 mix3 mix4;
do
	test -e "$mix" || echo "Bad ditectory structure. Aborting..."
	test -e "$mix" || exit 1
	cd "$mix" 

	for chan in 1 2 4 8 16 32 64 128 256;
	do
		test -e "$chan" || echo "Bad ditectory structure. Aborting..."
		test -e "$chan" || exit 1
		cd "$chan" 

		for config in prefetch noprefetch;
		do
			test -e "$config" || echo "Bad ditectory structure. Aborting..."
			test -e "$config" || exit 1
			cd "$config" 
			
			echo In directory for $mix/$chan/$config

			rm -rf `ls | grep -v HybridSim`

			test -e HybridSim || echo "Bad ditectory structure. Aborting..."
			test -e HybridSim || exit 1
			cd HybridSim


			rm -rf echo `ls |grep -v hybridsim.log|grep -v hybridsim_epoch.log|grep -v nvdimm_logs|grep -v out|grep -v err`

			# Leave HybridSim directory
			cd ..

			# Leave config directory
			cd ..
		done

		# Leave chan directory.
		cd ..
	done

	# Leave mix directory.
	cd ..
done

