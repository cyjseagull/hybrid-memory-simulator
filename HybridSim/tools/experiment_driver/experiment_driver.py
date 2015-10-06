import os

######################################################################
# Generic Hybrid Memory Experiment Driver Infrastructure
#
# Author: Jim Stevens
# 
# The infrastructure performs the following tasks:
# 1. Check out the simulators into the master directory.
# 2. Build DRAMSim2 and NVDIMMSim in the master directory.
# 3. Create the experiment directories and copy in the master directory.
# 4. Build HybridSim and marss in the experiment directories.
# 5. Run the appropriate simulator based on the mode.
#
# Usage:
# Subclass ExperimentHooks to configure the experiments.
# In the subclass, you can override hooks to perform tasks
# specific to a certain experiment type. These hooks can be called
# during the master directory setup, the specific experiment directory
# setup, and the running of the simulators.
#
# Commands:
# 1. run
# 2. cleanup (delete the simulators, save the results)
# 3. clobber (delete everything)
#
# Modes (i.e. types of experiments):
# 1. hybridsim
# 2. marss.hybridsim
# 
######################################################################


######################################################################
# globals

dry_run=True

######################################################################
# System Commands

# This class is used to build a chain of commands to perform some task.
# TODO: Come up with a good generic way to extract results (e.g. tmp files that can be read)
class Command(object):
	def __init__(self):
		self.command = ''

	def run(self):
		# Pass command chain to the OS and get the return value.
		retval = 0
		if not dry_run:
			retval = os.system(self.command)
		print '# Command object:'
		print self.command
		return retval

	def add(self, new_command):
		# Add a command to the chain.
		self.command += new_command + '\n'


	def echo(self, message):
		new_cmd = 'echo %s'%(message)
		self.add(new_cmd)

	def change_directory(self, directory_path):
		new_cmd = 'cd %s'%(directory_path)
		self.add(new_cmd)

	def git_clone(self, url, branch='master'):
		new_cmd = 'git clone -b %s %s'%(branch, url)
		self.add(new_cmd)

	def make_directory(self, directory_path):
		new_cmd = 'mkdir -p %s'%(directory_path)
		self.add(new_cmd)

	def copy_directory(self, path1, path2):
		new_cmd = 'cp -r %s %s'%(path1, path2)
		self.add(new_cmd)

	# sed on a specific file.
	# Note: escape characters must be handled by caller.
	def sed_file(self, file_path, orig_string, new_string):
		new_cmd = "sed -i 's/%s/%s/' %s"%(orig_string, new_string, file_path)
		self.add(new_cmd)

	def check_directory_exists(self, directory_path):
		new_cmd = 'test -e %s'%(directory_path)
		self.add(new_cmd)

	def remove_if_exists(self, path):
		new_cmd = 'test -e %s && rm -rf %s'%(path, path)
		self.add(new_cmd)
		
	def exit_on_fail(self, message):
		# If the last command did not return a status 0, then echo the message
		# and quit the currently running command sequence.
		new_cmd = 'test $? -eq 0 || (echo %s && exit 1)'%(message)
		self.add(new_cmd)


######################################################################
# Generic experiment driver 
# The hooks object passed in tells the driver how to run the experiment.

class ExperimentDriver(object):
	def __init__(self, hooks):
		self.hooks = hooks

		global dry_run
		dry_run = self.hooks.dry_run

		# Set flags based on the mode.
		if self.hooks.mode == 'marss.hybridsim':
			self.use_marss = True
			self.hybridsim_lib = True
		elif self.hooks.mode == 'hybridsim':
			self.use_marss = False
			self.hybridsim_lib = False
		else:
			raise Exception('Invalid mode set in hooks: %s'%(self.mode.hooks))

		# Save the path of the master directory.
		self.master_path = os.getcwd() + '/master'


	#########################################################
	# External interface methods
	def run(self):
		self.create_master_directory()
		self.run_all_experiments()

	def cleanup(self):
		# TODO: Implement the cleanup routine.
		# This will extract all results we care about into the results directory
		# in the experiment and then remove all of the simulator directories.
		pass


	#########################################################
	# master directory setup
	def create_master_directory(self):
		cmd = Command()
		cmd.echo('Building master directory...')
		if self.hooks.reuse_master:
			cmd.add('test -e master && (exit 0 && echo master directory will be reused.)')
		cmd.remove_if_exists('master')
		cmd.make_directory('master')
		cmd.change_directory('master')
		self.create_dramsim_directory(cmd)
		self.create_nvdimm_directory(cmd)
		self.create_hybridsim_directory(cmd)
		if self.use_marss:
			self.create_marss_directory(cmd)
		cmd.change_directory('..')
		cmd.run()

	def create_dramsim_directory(self, cmd):
		cmd.git_clone(self.hooks.dramsim_url, self.hooks.dramsim_branch)
		cmd.exit_on_fail('DRAMSim checkout failed.')
		cmd.change_directory('DRAMSim2')
		self.hooks.master_dramsim_hook(cmd)
		cmd.add('make libdramsim.so')
		cmd.exit_on_fail('DRAMSim failed to build.')
		cmd.change_directory('..')

	def create_nvdimm_directory(self, cmd):
		cmd.git_clone(self.hooks.nvdimm_url, self.hooks.nvdimm_branch)
		cmd.exit_on_fail('NVDIMM checkout failed.')
		cmd.change_directory('NVDIMMSim/src')
		self.hooks.master_nvdimm_hook(cmd)
		cmd.add('make lib')
		cmd.exit_on_fail('NVDIMM failed to build.')
		cmd.change_directory('../..')

	def create_hybridsim_directory(self, cmd):
		cmd.git_clone(self.hooks.hybridsim_url, self.hooks.hybridsim_branch)
		cmd.exit_on_fail('HybridSim checkout failed.')
		cmd.change_directory('HybridSim')
		self.hooks.master_hybridsim_hook(cmd)
		cmd.change_directory('..')

	def create_marss_directory(self, cmd):
		cmd.git_clone(self.hooks.marss_url, self.hooks.marss_branch)
		cmd.exit_on_fail('marss.hybridsim checkout failed.')
		cmd.change_directory('marss.hybridsim')
		self.hooks.master_marss_hook(cmd)
		cmd.change_directory('..')


	#########################################################
	# experiment running stuff

	def run_all_experiments(self):
		# Build the experiment_tuples list.
		experiment_tuples = self.generate_experiment_tuples(self.hooks.parameters_lists)

		# Variables to track our progress.
		self.setup_count = 0
		self.run_count = 0

		# Setup each experiment.
		for i in experiment_tuples:
			self.setup_experiment(i)
			self.setup_count += 1

		# Run each experiment.
		for i in experiment_tuples:
			self.run_experiment(i)
			self.run_count += 1

		cmd = Command()
		cmd.echo('Started %d experiments successfully'%(len(experiment_tuples)))
		cmd.run()

	def setup_experiment(self, cur_tuple):
		cmd = Command()
		cmd.echo('Setting up experiment %d: %s'%(self.setup_count, self.create_experiment_path(cur_tuple)))
		self.setup_experiment_directory(cmd, cur_tuple) # Note: this changes directory
		self.build_hybridsim(cmd, cur_tuple)
		self.build_marss(cmd, cur_tuple)
		self.return_to_top(cmd, cur_tuple)
		cmd.echo('Finished setting up experiment %d: %s'%(self.setup_count, self.create_experiment_path(cur_tuple)))
		cmd.add('sleep 1')
		cmd.run()

	def run_experiment(self, cur_tuple):
		cmd = Command()
		cmd.echo('Running experiment %d: %s'%(self.run_count, self.create_experiment_path(cur_tuple)))
		cur_path = self.create_experiment_path(cur_tuple)
		cmd.change_directory(cur_path)
		self.run_simulator(cmd, cur_tuple)
		self.return_to_top(cmd, cur_tuple)
		cmd.echo('Started running experiment %d: %s'%(self.run_count, self.create_experiment_path(cur_tuple)))
		cmd.echo('Sleeping %d seconds between starting runs to not overwhelm the server...'%self.hooks.sleep_time)
		cmd.add('sleep %d'%self.hooks.sleep_time)
		cmd.run()

	def setup_experiment_directory(self, cmd, cur_tuple):
		cur_path = self.create_experiment_path(cur_tuple)
		cmd.remove_if_exists(cur_path)
		cmd.make_directory(cur_path)
		cmd.change_directory(cur_path)
		self.copy_master(cmd, cur_path)
		self.call_experiment_hooks(cmd, cur_tuple)

	def create_experiment_path(self, cur_tuple):
		return '/'.join([str(i) for i in cur_tuple])

	def copy_master(self, cmd, cur_path):
		cmd.copy_directory(self.master_path+'/*', '.')

	def call_experiment_hooks(self, cmd, cur_tuple):
		# Call hooks (make sure to switch to the right path first)
		cmd.change_directory('DRAMSim2')
		self.hooks.experiment_dramsim_hook(cmd, cur_tuple)
		cmd.change_directory('..')

		cmd.change_directory('NVDIMMSim/src')
		self.hooks.experiment_nvdimm_hook(cmd, cur_tuple)
		cmd.change_directory('../..')

		cmd.change_directory('HybridSim')
		self.hooks.experiment_hybridsim_hook(cmd, cur_tuple)
		cmd.change_directory('..')

		if self.use_marss:
			cmd.change_directory('marss.hybridsim')
			self.hooks.experiment_marss_hook(cmd, cur_tuple)
			cmd.change_directory('..')
		
	def build_hybridsim(self, cmd, cur_tuple):
		# Build HybridSim
		cmd.change_directory('HybridSim')
		cmd.echo('Building HybridSim...')
		if self.hybridsim_lib:
			cmd.add('make lib')
		else:
			cmd.add('make')
		cmd.exit_on_fail('HybridSim failed to build.')
		cmd.change_directory('..')

	def build_marss(self, cmd, cur_tuple):
		# Build marss (if marss mode)
		if self.use_marss:
			cmd.echo('Building marss...')
			cmd.change_directory('marss.hybridsim')
			build_cmd = 'scons c=%s -j16 dramsim=`pwd`/../HybridSim'%(self.hooks.core_count)
			cmd.add(build_cmd)
			cmd.exit_on_fail('marss.hybridsim failed to build.')
			cmd.add('ldd qemu/qemu-system-x86_64 | grep hybridsim')
			cmd.exit_on_fail('HybridSim failed to link into marss.')
			cmd.change_directory('..')

	def run_simulator(self, cmd, cur_tuple):
		if self.use_marss:
			# Run marss (if marss mode)
			self.run_marss(cmd, cur_tuple)
		else:
			# Run HybridSim (if hybridsim mode)
			self.run_hybridsim(cmd, cur_tuple)

	def run_marss(self, cmd, cur_tuple):
		cmd.change_directory('marss.hybridsim')
		cmd.echo('Running marss...')
		self.hooks.simconfig_setup_hook(cmd, cur_tuple)
		self.hooks.run_marss_hook(cmd, cur_tuple)
		cmd.change_directory('..')

	def run_hybridsim(self, cmd, cur_tuple):
		cmd.change_directory('HybridSim')
		cmd.echo('Running HybridSim...')
		self.hooks.run_hybridsim_hook(cmd, cur_tuple)
		cmd.change_directory('..')


	# Generate a list of paths for the experiments
	def generate_experiment_tuples(self, parameters_lists):
		# Parameters lists is a list of lists. Each sublist consists of
		# strings or numbers 
		#print '------------------------------------'
		#print 'parameters_lists',parameters_lists
		#print 'parameters_lists[1:]',parameters_lists[1:]

		partials = []
		tuples = []

		if len(parameters_lists) > 1:
			#print 'partials'
			partials = self.generate_experiment_tuples(parameters_lists[1:])

			for i in partials:
				for j in parameters_lists[0]:
					cur_tuple = [j]
					cur_tuple.extend(i)
					tuples.append(cur_tuple)
		else:
			for j in parameters_lists[0]:
				cur_tuple = [j]
				tuples.append(cur_tuple)

		return tuples

	def return_to_top(self, cmd, cur_tuple):
		cmd.echo('Returning to top directory...')
		# Generate a '..' for each level in the experiment directory path.
		relative_path = '/'.join(['..' for i in cur_tuple])
		cmd.change_directory(relative_path)

######################################################################
# ExperimentHooks class defines default experimental configuration.
# It is defined to be overriden by a child class to create experiments.

class ExperimentHooks(object):
	def __init__(self):
		# Set up configuration parameters.

		# Note: you can override any of these parameters in a child class
		# but you should call the following method to set up the defaults...
		#super(classname, self).__init__()

		# Set up git repo URLs and desired branches.
		self.dramsim_url = 'https://github.com/jimstevens2001/DRAMSim2.git'
		self.dramsim_branch = 'master'
		self.nvdimm_url = 'https://github.com/jimstevens2001/NVDIMMSim.git'
		self.nvdimm_branch = 'stable'
		self.hybridsim_url = 'https://github.com/jimstevens2001/HybridSim.git'
		self.hybridsim_branch = 'dev'
		self.marss_url = 'https://github.com/jimstevens2001/marss.hybridsim.git'
		self.marss_branch = 'features'

		# valid modes:
		# marss.hybridsim
		# hybridsim
		#self.mode = 'marss.hybridsim'
		self.mode = 'hybridsim'

		# parameters_lists is used to create the directory structure and
		# specify the specific parameters for each experiment.
		params1 = ['a','b','c']
		params2 = [1,2,3,4]
		params3 = ['x','y','z']
		self.parameters_lists = [['rundir'], params1, params2, params3]

		# reuse_master tells ExperimentDriver to not delete the old master directory
		# if it is around. This is useful for debugging an experiment.
		# Note that this will NOT check to make sure that the master directory contains the 
		# correct subdirectories or that those subdirectories are up to date.
		self.reuse_master = False

		# dry_run tells ExperimentDriver to only print out the commands being generated and not run them
		self.dry_run = True

		# execute tells the ExperimentDriver actually run the simulator for each experiment.
		# If this is false, then the ExperimentDriver will build all of the experiment directories
		# but not actually run them.
		# This is useful to build the directories and then manually run the experiments for testing purposes.
		self.run_simulator = True

		# Sleep time between each run (to keep from overloading the system).
		self.sleep_time = 30

		# Nice level to run with the simulator.
		self.nice = 10

		# trace-based mode only parameters
		# Note: this can always be overriden in the experiment_hybridsim_hook().
		self.trace_file = 'traces/test.txt'


		# marss mode only parameters
		# Note: this can always be overridden in the experiment_marss_hook().
		disk_base = '/home/jims/code/disks/'
		self.disk_file = disk_base+'PARSEC_32G.qcow2'
		self.snapshot_name = 'canneal_16_large'
		self.memory_size = '32G'
		self.core_count = '8'
		self.machine_type = 'shared_l3'
		self.machine_hz = '2000000000'
		self.stopinsns = '' # Note: 1M is million and 1G is billion
		self.vnc_port = 1


	#########################################################
	# Hooks for configuration for ALL experiment runs.

	def master_dramsim_hook(self, cmd):
		cmd.echo('In generic master_dramsim_hook')
	def master_nvdimm_hook(self, cmd):
		cmd.echo('In generic master_nvdimm_hook')
	def master_hybridsim_hook(self, cmd):
		cmd.echo('In generic master_hybridsim_hook')
	def master_marss_hook(self, cmd):
		cmd.echo('In generic master_marss_hook')

	#########################################################
	# Hooks for configuration of SPECIFIC experiment runs.
	# Experiment parameters are available in cur_tuple.
	# These run during experiment SETUP!

	def experiment_dramsim_hook(self, cmd, cur_tuple):
		cmd.echo('In generic experiment_dramsim_hook')
	def experiment_nvdimm_hook(self, cmd, cur_tuple):
		# e.g. NVDIMM ini replacements with sed
		cmd.echo('In generic experiment_nvdimm_hook')
	def experiment_hybridsim_hook(self, cmd, cur_tuple):
		# e.g. HybridSim ini replacement with sed
		cmd.echo('In generic experiment_hybridsim_hook')
	def experiment_marss_hook(self, cmd, cur_tuple):
		# e.g. MARSS config replacements with sed
		cmd.echo('In generic experiment_marss_hook')

	#########################################################
	# Hooks for running the simulators.
	# Experiment parameters are available in cur_tuple.
	# These run during the RUNNING of an experiment.

	def simconfig_setup_hook(self, cmd, cur_tuple):
		# See options here: http://marss86.org/~marss86/index.php/Run-time_Configuration
		# Full definition in "ptlsim/sim/ptlsim.cpp"
		cmd.echo('-corefreq %s > simconfig'%(self.machine_hz))
		cmd.echo('-machine %s >> simconfig'%(self.machine_type))
		cmd.echo('-logfile marss.log >> simconfig')
		cmd.echo('-stats stats.log >> simconfig')
		if self.stopinsns != '':
			cmd.echo('-stopinsns %s >> simconfig'%self.stopinsns)
		cmd.echo('-run >> simconfig')
		cmd.echo('-kill-after-run >> simconfig')

	def run_marss_hook(self, cmd, cur_tuple):
		marss_cmd = 'nohup nice -n %d qemu/qemu-system-x86_64 -snapshot -vnc :%d -simconfig simconfig -m %s -loadvm %s %s > out 2> err &'
		marss_cmd = marss_cmd%(self.nice, self.vnc_port, self.memory_size, self.snapshot_name, self.disk_file)
		if not self.run_simulator:
			marss_cmd = '#'+marss_cmd
		cmd.add(marss_cmd)
		self.vnc_port += 1

	def run_hybridsim_hook(self, cmd, cur_tuple):
		hybridsim_cmd = 'nohup nice -n %d ./HybridSim %s > out 2> err &'%(self.nice, self.trace_file)
		if not self.run_simulator:
			hybridsim_cmd = '#'+hybridsim_cmd
		cmd.add(hybridsim_cmd)
		

if __name__ == '__main__':
	eh = ExperimentHooks()
	ed = ExperimentDriver(eh)
	ed.run()


