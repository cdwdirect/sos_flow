#!/usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python

import sys
import time
import os
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.cm as cm
from matplotlib.colors import Normalize
import matplotlib.pyplot as plt
import numpy as np
from ssos import SSOS


def queryAndPlot():
    SOS = SSOS()

    print "Initializing SOS..."
    SOS.init()

    sql_string = """
    SELECT MAX(frame) FROM viewCombined WHERE viewCombined.value_name LIKE "lulesh.time"
    ;
    """
    results, col_names = SOS.query(sql_string,
            "localhost",
            os.environ.get("SOS_CMD_PORT"))
    
    max_cycle = int(results[0][0])
    print "Max cycle: " + str(max_cycle)

    sql_string = """
    SELECT
    comm_rank,
    frame,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.time" THEN value END) AS time,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.dtime" THEN value END) AS dtime,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.coords" THEN value END) AS coords,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.scalar" THEN value END) AS scalar
    FROM viewCombined
    WHERE frame = """ + str(max_cycle) + """
    GROUP BY
    comm_rank, frame
    ;
    """

    #print "Sending this query to the SOS daemon: "
    #print "    " + sql_string
    results, col_names = SOS.query(sql_string, "localhost", os.environ.get("SOS_CMD_PORT"))
    print "Results:"
    print str(col_names)
    print str(results)

    attr = dict()
    attr['comm_rank']  =  [el[0] for el in results]
    attr['cycle']      =  [el[1] for el in results]
    attr['iter_time']  =  [el[2] for el in results]
    attr['delta_time'] =  [el[3] for el in results]
    res_coords         =  [el[4] for el in results]
    attr['scalar']     =  [el[5] for el in results]

    rank_max = len(attr['comm_rank'])
    coords = list()
    coords = [el.split() for el in res_coords]

    ###
    ### NOTE: Some matplot lib stuff...
    ###
    #norm = Normalize(vmin=0.0000001, vmax=0.000001)
    
    if (os.environ.get("DISPLAY") is not None):
        print "Rendering a plot using matplotlib..."
        colorset = cm.rainbow
        fig = plt.figure()
        ax = fig.add_subplot(111, projection='3d')

        for rank in range(rank_max):
            xpt = [None]*8
            ypt = [None]*8
            zpt = [None]*8
            cpt = [None]*8
            for i in range(8):
                xpt[i] = float(coords[rank][(i * 3)])
                ypt[i] = float(coords[rank][(i * 3) + 1])
                zpt[i] = float(coords[rank][(i * 3) + 2])
                cpt[i] = xpt[i] + ypt[i] + zpt[i]
            ax.scatter(xpt, ypt, zpt, s=200, c=cpt, cmap=colorset, marker='s')
            ax.set_xlabel('X')
            ax.set_ylabel('Y')
            ax.set_zlabel('Z')

        plt.title('LULESH step = ' + str(max_cycle))
        plt.grid(True)
        plt.draw()
        #print "Saving image ..."
        #plt.savefig("./imgs/lulesh_" + str(max_cycle) + ".png")
        plt.show()
    else:
        print "There is no DISPLAY variable set, skipping rendering." 

    SOS.finalize();
    print "   ...DONE!"
    print 
    #############
  
if __name__ == "__main__":
    queryAndPlot()
    #############



