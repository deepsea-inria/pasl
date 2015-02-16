import matplotlib.pyplot as plt
import matplotlib.cm as cmx
import matplotlib.colors as colors

def get_cmap(N):
    '''Returns a function that maps each index in 0, 1, ... N-1 to a distinct 
    RGB color.'''
    color_norm  = colors.Normalize(vmin=0, vmax=N-1)
    scalar_map = cmx.ScalarMappable(norm=color_norm, cmap='hsv') 
    def map_index_to_rgb_color(index):
        return scalar_map.to_rgba(index)
    return map_index_to_rgb_color	

if __name__ == "__main__":
	inf = open("LOG.log", 'r')
	
	plots = dict() # 3: (1 : (0:4))
	shared_plot = dict()
	max_color = -1
	max_update_y = -1
	max_report_y = -1
	
	for line in inf.readlines():
		a = line.split('\t')
		a = list(filter(None, [a[i].strip(' \t\n\r') for i in range(len(a))]))
		if (a[2] == 'estim_update'):
			if plots.get(a[3]) == None:
				plots[a[3]] = dict()
			if plots[a[3]].get(a[1]) == None:
				plots[a[3]][a[1]] = dict()
			max_color = max(max_color, int(a[1]))
			plots[a[3]][a[1]][float(a[0])] = float(a[4])
		if (a[2] == 'estim_update_shared'):
			if shared_plot.get(a[3]) == None:
				shared_plot[a[3]] = dict()
			shared_plot[a[3]][float(a[0])] = float(a[4])
				
	max_color += 2
	cmap = get_cmap(max_color)
	for ofname in plots.keys():
		plt.xlabel('Time')
		plt.ylabel('Constant')
		plt.title(ofname)	
		
		cur_color = 0
		leg = []
		for procname in plots[ofname]:
			x = sorted(plots[ofname][procname].keys())
			y = [plots[ofname][procname][i] for i in x]
			plt.plot(x, y, '-', color=cmap(cur_color))
			leg.append(procname)
			cur_color += 1
			
		x = sorted(shared_plot[ofname].keys())
		y = [shared_plot[ofname][i] for i in x]
		plt.plot(x, y, '^k')
		leg.append('shared')
		plt.legend(leg, loc='best')
		plt.savefig(ofname +".png")
		plt.close()