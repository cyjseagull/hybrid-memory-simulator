from experiment_driver import *

class MyHooks(ExperimentHooks):
	def __init__(self):
		# Call the parent class constructor.
		super(MyHooks, self).__init__()

		self.mode = 'marss.hybridsim'

		npb_disk = 'NPB_32G'
		npb_snapshots = ['sp_C_16', 'is_C_16', 'ft_B_16', 'bt_C_16', 'lu_C_16', 'ft_C_16', 'bt_B_16']
		npb_runs = [npb_disk+'___'+i for i in npb_snapshots]

		parsec_disk = 'PARSEC_32G'
		parsec_snapshots = ['fluidanimate_16_large', 'canneal_16_large', 'bodytrack_16_large', 'facesim_16_large', 
				'fluidanimate_8_large', 'canneal_8_large', 'bodytrack_8_large', 'facesim_8_large']
		parsec_runs = [parsec_disk+'___'+i for i in parsec_snapshots]

		all_runs = npb_runs + parsec_runs

		self.parameters_lists = [['trace_run'], all_runs]
		self.dry_run = True
		self.sleep_time = 60
		self.stopinsns = '2G'

	def master_hybridsim_hook(self, cmd):
		cmd.echo('In gen_traces master_hybridsim_hook')

		# Set the overall size and cache size to 32 GB.
		cmd.sed_file('ini/hybridsim.ini', 'CACHE_PAGES=131072', 'CACHE_PAGES=8388608')
		cmd.sed_file('ini/hybridsim.ini', 'TOTAL_PAGES=2097152', 'TOTAL_PAGES=8388608')

		# Enable the trace dump from HybridSim.
		cmd.sed_file('config.h', '#define DEBUG_FULL_TRACE 0', '#define DEBUG_FULL_TRACE 1')

	def run_marss_hook(self, cmd, cur_tuple):
		cmd.echo('In gen_traces run_marss_hook')

		# Make the NVDIMM log directory (since this keeps failing at runtime).
		cmd.make_directory('nvdimm_ps_logs')

		# Unpack the parameters as the current disk and snapshot.
		unpacked =  cur_tuple[1].split('___')
		self.disk_file = '/home/jims/code/disks/'+unpacked[0]+'.qcow2'
		self.snapshot_name = unpacked[1]
		cmd.echo('Setting disk_file and snapshot_name for this experiment...')

		# Run marss using the parent's run_marss_hook() method.
		super(MyHooks, self).run_marss_hook(cmd, cur_tuple)

if __name__ == '__main__':
	eh = MyHooks()
	ed = ExperimentDriver(eh)
	ed.run()

