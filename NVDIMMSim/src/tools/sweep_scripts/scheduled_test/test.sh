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
git clone https://watchmaker@github.com/watchmaker/NVDIMMSim.git
cd NVDIMMSim/src
make lib
cd ../..

# Checkout HybridSim
git clone https://github.com/jimstevens2001/HybridSim.git

# Go back to top level directory
cd ..


for mix in mix1 mix2 mix3 mix4;
do
	# Clear old stuff
	rm -rf "$mix"

	# If the mix directory doesn't exist, then create it.
	test -e "$mix" || mkdir "$mix"
	cd "$mix"

	for chan in 1 2 4 8 16 32;
	do

	    # If the channel directory doesn't exist, then create it.
	    test -e "$chan"|| mkdir "$chan"
	    cd "$chan"

	    for qlength in 2 8 16;
	    do
		# If the prefetch option directory doesn't exist, then create it.
		test -e "$qlength" || mkdir "$qlength"
		cd "$qlength"
			
		echo In directory for $mix/$chan/$qlength

		# Copy repos
		cp -r $topdir/master/* .

		cd HybridSim

		# Copy config.h and ini files for this experiment.
		cp $topdir/ini/"$chan"_"$qlength"_sched.ini ini/samsung_K9XXG08UXM_mod.ini
		cp $topdir/ini/hybridsim.ini ini/hybridsim.ini
		cp $topdir/ini/TraceBasedSim.cpp .
		
		
		# Copy fast forwardig state		
		scp -r pkt3c@naan.ece.umd.edu:/home/ibhati/disk_ff_"$mix"/state .

		# Copy trace to run.
		cp $topdir/original/"$mix".txt traces/"$mix".txt

		# Build HybridSim
		make

		# Run Experiment
		nohup nice -5 ./HybridSim traces/"$mix".txt > out 2> err &

		# Leave HybridSim directory
		cd ..
		
		# Leave Speed directory
		cd ..
	    done

	    # Leave chan directory.
	    cd ..
	done

	# Leave mix directory.
	cd ..
done

echo All experiments are running.
