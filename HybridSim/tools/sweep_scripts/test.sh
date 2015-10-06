topdir=`pwd`

# Make master directory
rm -rf master
mkdir master
cd master


# Checkout and make DRAMSim
git clone -b tmp https://github.com/jimstevens2001/DRAMSim2.git
cd DRAMSim2
make libdramsim.so
cd ..

# Checkout and make NVDIMM.
git clone https://github.com/jimstevens2001/NVDIMMSim.git
cd NVDIMMSim/src
make lib
cd ../..

# Checkout HybridSim
git clone http://github.com/jimstevens2001/HybridSim.git

# Go back to top level directory
cd ..


for mix in mix1 mix2 mix3 mix4;
do
	# Clear old stuff
	rm -rf "$mix"

	# If the mix directory doesn't exist, then create it.
	test -e "$mix" || mkdir "$mix"
	cd "$mix"

	for chan in 1 2 4 8 16 32 64 128 256;
	do
		# If the channel directory doesn't exist, then create it.
		test -e "$chan"|| mkdir "$chan"
		cd "$chan"

		for config in noprefetch prefetch;
		do
			# If the prefetch option directory doesn't exist, then create it.
			test -e "$config" || mkdir "$config"
			cd "$config"
			
			echo In directory for $mix/$chan/$config

			# Copy repos
			cp -r $topdir/master/* .

			cd HybridSim

			# Copy config.h and ini files for this experiment.
			cp $topdir/ini/"$config".h config.h
			cp $topdir/ini/"$chan"_chan.ini ini/samsung_K9XXG08UXM_mod.ini
			cp $topdir/ini/hybridsim.ini ini/hybridsim.ini
			cp $topdir/ini/TraceBasedSim.cpp .

			# Copy fast forwarding state
			test -e state || mkdir state
			scp -r naan:/home/ibhati/disk_ff_"$mix"/state .

			# Copy trace to run.
			scp naan:~/ishwar_script/traces/original/"$mix".txt traces/"$mix".txt

			# Copy prefetch data 
			#if [ "$config" == 'prefetch' ]; then
			#scp naan:~/ishwar_script/traces/prefetch2/"$mix"/prefetch_data.txt traces/prefetch_data.txt
			scp naan:~/ishwar_script/traces/prefetch2/"$mix"/prefetch_cache_state.txt state/my_state.txt
			#fi;

			# Build HybridSim
			make

			# Run Experiment
			nohup nice -5 ./HybridSim traces/"$mix".txt > out 2> err &

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

echo All experiments are running.
