def read_file():
	f = open('config.txt')
	info = {}
	for line in f.readlines():
		if line == '\n' or '#' in line: continue
		else:
			tokens = line.split('=')
			info[tokens[0]] = tokens[1][:-1]
	return info
	
def config(): return read_file()['config']
def identifier(): return read_file()['ident']
def start_run(): return int(read_file()['start_run'])
def end_run(): return int(read_file()['end_run'])
def nr_events(): return int(read_file()['n_events'])
def uva_id(): return read_file()['uva_id']
def jlab_id(): return read_file()['jlab_id']
def e_cut(): return int(read_file()['e_cut'])
def ident_output(): return read_file()['output']

def subassems():
	assem_string = read_file()['subassemblies']
	return assem_string.split(',')
