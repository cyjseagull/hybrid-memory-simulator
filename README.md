Hybrid-Memory-Simulator : MARSSX86 && NVMain && DRAMSim2 && HybridSim 
=====================================================================

Copyright 2015 Huazhong University of Science and Technology (cyjseagull @ 163.com)

Hybrid-Memory-Simulator is a simulation tool for x86-64 based computing system with hybrid memory system consisting of both non-volatile memory(such as SSD , PCM ) and dynamic random access memory. 

It is based on four independent open-source project: MARSSx86(http://www.marss86.org/~marss86/index.php/Home), NVMain(http://wiki.nvmain.org/) , DRAMsim2(https://github.com/jimstevens2001/DRAMSim2), NVDIMMSim(nvm simulator,but not very accurate , https://github.com/jimstevens2001/NVDIMMSim), HybridSim(https://github.com/jimstevens2001/HybridSim).Thanks for the hard working of people who had developed these simulators and made them open-source.  

System Requirements
-------------------
To compile Marss on your system, you will need following:
* 2.5GHz CPU with minimum 2GB RAM (4GB Preferred)
* Standard C++ compiler, like g++ or icc
* SCons tool for compiling Marss (Minimum version 1.2.0)
* SDL Development Libraries (Required for QEMU)


Compiling
---------
If you do not have SCons install, install it using your stanard application
installation program like apt-get or yum.

Once you have SCons install go to the code hybrid-memory-simulator and give following command:

		`$scons -Q config=machine configuration path  [mem_type=NVMAIN/DRAMSIM/HYBRIDSIM] [mem_dir=path of memory simulator] [debug=0/1/2] [c=num of cores]`

* config: required , path of machine config file
* mem_type : optional , default is `NVMAIN` ( plug NVMain as memory system simulator)
* mem_dir : optional , default is `hybrid-memory-simulator/nvmain` ( if mem_dir is default , make sure nvmain simulator exists in hybrid-memory-simulator directory)
* debug : optional , default is `0` (no debugging)
* c: optional , default is `1`, single-core configuration
* attention : 
>> `NVMAIN` as `mem_type` , default `mem_dir` is `hybrid-memory-simulator/nvmain`
>> `DRAMSIM` as `mem_type` , default `mem_dir` is `hybrid-memory-simulator/DRAMSim2`
>> `HYBRIDSIM` as `mem_type` , default `mem_dir` is `hybrid-memory-simulator/HybridSim`
>> before compile hybrid-memory-system , you must make sure `libdramsim.so` in `DRAMSim2 directory and libhybridsim.so` in `HybridSim directory` 

e.g.:
		`$ scons -Q mem=NVMAIN config=config/new-machine/moesi.conf c=2`

To clean your compilation:
		`$ scons -Q -c`

Running
-------
After successfull compilation, to run hybrid-memory-simulator , go to hybrid-memory-simulator directory , then give the following command:

    `$ qemu/qemu-system-x86_64 -m [memory_size] -hda [path-to-qemu-disk-image] -simconfig [simconfig file path] [-nographic]`
* `-m`: optional , memory size , unit is `M` or `G` or `K` 
* `-hda` : required , path of qemu disk image 
* `-simconfig`: optional , path of simconfig file , simconfig file is used to config simulation , such as logfile,nvmain_config_path , loglevel , configuration related to dramsim2 , etc. You can know it deeply through MARSSX86 webpage(http://www.marss86.org/~marss86/index.php/Home). You can also switch to simulation mode , and set simconfig according to following step.
* `-nographic`: when connect remote server with shell , and run qemu through shell , should add this option to disable graphic requirement.

You can use all the regular QEMU command here, like start VM window in VNC give
`vnc :10` etc.  Once the system is booted, you can switch to Monitor mode using
`Ctrl-Alt-2` key and give following command to switch to simulation mode:

    `(qemu) simconfig -run -stopinsns 100m -stats [stats-file-name]`

To get the list of available simulation options give following command:

    `(qemu) simconfig`

It will print all the simulation options on STDOUT.

Happy Hacking.
