topdir=`pwd`


for mix in mix1 mix2 mix3 mix4;
do
	cd "$mix"

	for chan in 1 2 4 8 16 32 64 128 256;
	do
		cd "$chan"

		for config in noprefetch;
		do
			cd "$config"
			
			#echo In directory for $mix/$chan/$config

			rm -rf `ls | grep -v HybridSim`

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

