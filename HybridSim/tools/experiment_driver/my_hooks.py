from experiment_driver import *

class MyHooks(ExperimentHooks):
	def __init__(self):
		# Call the parent class constructor.
		super(MyHooks, self).__init__()

		self.mode = 'marss.hybridsim'

		#self.parameters_lists = [['rundir'], ['a', 'b', 'c']]
		self.parameters_lists = [['rundir'], ['a']]
		#self.dry_run = False
		self.dry_run = True

	def experiment_hybridsim_hook(self, cmd, cur_tuple):
		cmd.echo('In MyHooks experiment_hybridsim_hook')

if __name__ == '__main__':
	eh = MyHooks()
	ed = ExperimentDriver(eh)
	ed.run()

