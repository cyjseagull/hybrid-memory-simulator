topdir=`pwd`

# Go back to top level directory
cd ..


for mix in mix1 mix2 mix3 mix4;
do
	cd "$mix"

	for chan in 1 2 4 8 16 32 64 128 256;
	do
		cd "$chan"

		for config in noprefetch prefetch;
		do
			cd "$config"
			
			#echo In directory for $mix/$chan/$config

			cd HybridSim

			if [ "$1" == 'done' ]; then
				test -e hybridsim.log && echo $mix - $chan - $config - `grep "total accesses:" hybridsim.log`
			else
				test -e hybridsim.log || echo $mix - $chan - $config is not done 
				test -e hybridsim.log || tail --lines=2 out
				test -e hybridsim.log || echo
				test -e hybridsim.log || echo
			fi

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

