topdir=`pwd`

# Go back to top level directory
cd ..


for mix in mix1 mix2 mix3 mix4;
do
	cd "$mix"

	for chan in 1 2 4 8 16 32;
	do
		cd "$chan"

		for speed in 1 2 4;
		do
			cd "$speed"
			
			#echo In directory for $mix/$chan/$speed

			cd nvdimm_logs

			if [ "$1" == 'done' ]; then
				test -e NVDIMM.log && echo $mix - $chan - $speed - `grep "total accesses:" NVDIMM.log`
			else
				test -e NVDIMM.log || echo $mix - $chan - $speed is not done 
				test -e NVDIMM.log || tail --lines=2 out
				test -e NVDIMM.log || echo
				test -e NVDIMM.log || echo
			fi

			# Leave nvdimm_logs directory
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

