#! /bin/bash
##This script will create a directory and run a test in that directory with MARSS.

EXPERIMENT=HybridRegress
MIXNUM=1

rm -rf $EXPERIMENT
mkdir $EXPERIMENT
cd $EXPERIMENT

git clone -b tmp https://github.com/jimstevens2001/DRAMSim2.git
cd DRAMSim2
make libdramsim.so
cd ..


git clone https://github.com/jimstevens2001/NVDIMMSim.git
cd NVDIMMSim/src
make lib
cd ../..


git clone -b tmp http://github.com/jimstevens2001/HybridSim.git
cd HybridSim
make lib
cd ..

git clone  http://github.com/jimstevens2001/marss.hybridsim.git
cd marss.hybridsim
scons c=4 pretty=0 dramsim=`pwd`/../HybridSim
ldd qemu/qemu-system-x86_64 | grep hybridsim
cd ..

mkdir parsec_roi
#cp /home/ibhati/disk_ff_mix1/parsecROI.qcow2 parsec_roi/
cp /home/ibhati/disk_ff_mix$MIXNUM/parsecROI.qcow2 parsec_roi/
cp /home/ibhati/disk_images/run_bench.py marss.hybridsim/

cp -r /home/ibhati/disk_ff_mix$MIXNUM/state HybridSim/
cp -r /home/ibhati/disk_ff_mix$MIXNUM/state marss.hybridsim/

#cp /home/ibhati/Regression_scripts/hybridsim.ini HybridSim/ini/
#cp /home/ibhati/Regression_scripts/samsung_K9XXG08UXM_mod.ini HybridSim/ini/
cp ../ini/hybridsim.ini HybridSim/ini/
cp ../ini/samsung_K9XXG08UXM_mod.ini HybridSim/ini/

cd marss.hybridsim
./run_bench.py
