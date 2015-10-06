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
git clone http://github.com/watchmaker/HybridSim.git

# Go back to top level directory
cd ..


for mix in mix1 mix2 mix3 mix4;
do
	# Clear old stuff
	rm -rf "$mix"

	# If the mix directory doesn't exist, then create it.
	test -e "$mix" || mkdir "$mix"
	cd "$mix"

	for chan in 2 4 16 32;
	do

	    # If the channel directory doesn't exist, then create it.
	    test -e "$chan"|| mkdir "$chan"
	    cd "$chan"

	    for speed in 1 2;
	    do
		
		 # If the speed directory doesn't exist, then create it.
		test -e "$speed" || mkdir "$speed"
		cd "$speed"

		for buffered in buffer nobuffer
		do
		    
		     # If the buffer option directory doesn't exist, then create it.
		    test -e "$buffered" || mkdir "$buffered"
		    cd "$buffered"
			
		    echo In directory for $mix/$chan/$speed/$buffered

		    # Copy repos
		    cp -r $topdir/master/* .
		    
		    cd HybridSim

		    # Copy config.h and ini files for this experiment.
		    cp $topdir/ini/"$chan"_"$speed"_chan_"$buffered".ini ini/samsung_K9XXG08UXM_mod.ini
		    cp $topdir/ini/hybridsim.ini ini/hybridsim.ini
		    cp $topdir/ini/TraceBasedSim.cpp .
		
		
		    # Copy fast forwardig state
		    test -e state || mkdir state
		    if [ $mix == mix1 ]
		    then
			cp -r $topdir/../../../../../States/Mix1/state_"$chan"pkg_4die_1plane/* state
		    elif [ $mix == mix2 ]
		    then
			cp -r $topdir/../../../../../States/Mix2/state_"$chan"pkg_4die_1plane/* state
		    elif [ $mix == mix3 ]
		    then
			cp -r $topdir/../../../../../States/Mix3/state_"$chan"pkg_4die_1plane/* state
		    elif [ $mix == mix4 ]
		    then
			cp -r $topdir/../../../../../States/Mix4/state_"$chan"pkg_4die_1plane/* state
		    fi

		    # Copy trace to run.
		    cp $topdir/original/"$mix".txt traces/"$mix".txt

		    # Build HybridSim
		    make

		    # Run Experiment
		    nohup ./HybridSim traces/"$mix".txt > out 2> err &

		    # Leave HybridSim directory
		    cd ..
		
		    # Leave Buffered directory
		    cd ..
		done

		# Leave the Speed directory
		cd ..
	    done

	    # Leave chan directory.
	    cd ..
	done

	# Leave mix directory.
	cd ..
done

echo All experiments are running.
