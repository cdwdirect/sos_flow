#!/usr/bin/python

import sys
import time
import os
import matplotlib.cm as cm
from matplotlib.colors import Normalize
import matplotlib.pyplot as plt
import numpy as np
from ssos import SSOS


def queryAndPlot():
    SOS = SSOS()

    print "Initializing SOS..."
    SOS.init()
    
    #GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.cycle" THEN value END) AS cycle,
    
    sql_string = """
    SELECT
    comm_rank,
    frame,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.time" THEN value END) AS time,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.dtime" THEN value END) AS dtime,
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.coords" THEN value END) AS coords
    FROM viewCombined
    GROUP BY
    comm_rank, frame;
    """

    print "Sending this query to the SOS daemon: "
    print "    " + sql_string
    results, col_names = SOS.query(sql_string, "localhost", os.environ.get("SOS_CMD_PORT"))
    print "Results:"

    attr = dict()
    attr['comm_rank']  =  [el[0] for el in results]
    attr['cycle']      =  [el[1] for el in results]
    attr['iter_time']  =  [el[2] for el in results]
    attr['delta_time'] =  [el[3] for el in results]
    res_coords         =  [el[4] for el in results]
    
    coords = list()
    coords = [el.split() for el in res_coords]
    rank_max = len(attr['comm_rank'])

    print "Coordinates at position 0:"
    print "p0.X=" + str(coords[0][0])
    print "p0.Y=" + str(coords[0][1])
    print "p0.Z=" + str(coords[0][2])

    print "Iteration time at position 0:"
    print str(attr['iter_time'][0])

    ###
    ### NOTE: Some matplot lib stuff...
    ###
    # colorset = cm.rainbow
    # norm = Normalize(vmin=0, vmax=10)
    # plt.ylim(0.0, 0.00001)
    # plt.xlim(-1.0, 20.0)
    # plt.scatter(res_cycle, res_time, c=norm(res_rank), cmap=colorset)
    # plt.xlabel('iteration')
    # plt.ylabel('time (s)')
    # plt.title('Lulesh Time per Iteration')
    # plt.grid(True)
    # plt.savefig("lulesh_time_per_iter.png")
    # plt.show()

    SOS.finalize();
    print "   ...DONE!"
    print 
    #############
  
if __name__ == "__main__":
    queryAndPlot()
    #############



