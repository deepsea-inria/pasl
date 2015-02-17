import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.cm as cmx
import matplotlib.colors as colors
import sys
import argparse
import os
#import os.pat%

def get_cmap(N):
    '''Returns a function that maps each index in 0, 1, ... N-1 to a distinct 
    RGB color.'''
    color_norm  = colors.Normalize(vmin=0, vmax=N-1)
    scalar_map = cmx.ScalarMappable(norm=color_norm, cmap='hsv') 
    def map_index_to_rgb_color(index):
        return scalar_map.to_rgba(index)
    return map_index_to_rgb_color	

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--dir', default='.', dest = 'dir')
    parser.add_argument('-p', '--prefix', default = '', dest = 'prefix', help='prefix of the files to be written')
    parser.add_argument('-proc', dest='proc', help='the number of proc')
    parser.add_argument('-b', '--binary', help='read from binary', action='store_true')
    args = parser.parse_args()

    inf = open(args.dir + "LOG", 'r')

    names = dict()
    
    plots = dict() # 3: (1 : (0:4))
    shared_plot = dict()
    max_color = -1
    max_update_y = -1
    max_report_y = -1
    
    for line in inf.readlines():
        a = line.split('\t')
        a = list(filter(None, [a[i].strip(' \t\n\r') for i in range(len(a))]))
        if (a[2] == 'estim_name'):
            names[a[3]] = a[4]
        a[3] = names[a[3]]
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
                
    if not os.path.exists(args.dir + "plots"):
        os.mkdir(args.dir + "plots")

    max_color += 2
    cmap = get_cmap(max_color)
    for ofname in plots.keys():
        if not os.path.exists(args.dir + "plots/" + args.proc):
            os.mkdir(args.dir + "plots/" + args.proc)
        if not os.path.exists(args.dir + "plots/" + args.proc + "/" + ofname):
            os.mkdir(args.dir + "plots/" + args.proc + "/" + ofname)

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
        plt.savefig(args.dir + "plots/" + args.proc + "/" + ofname + "/" + args.prefix + ".png")
        plt.close()