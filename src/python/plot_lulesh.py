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
    GROUP_CONCAT(CASE WHEN value_name LIKE "lulesh.coords" THEN value END) AS coords,
    node_id
    FROM viewCombined
    GROUP BY
    comm_rank, frame;
    """

    print "Sending this query to the SOS daemon: "
    print "    " + sql_string
    results, col_names = SOS.query(sql_string, "localhost", os.environ.get("SOS_CMD_PORT"))
    print "Results:"
 
    print str(col_names)
    print str(results)

    #res = np.zeros((len(results), len(col_names)))
    #for idx_row, row in enumerate(results):
    #    for idx_col, col in enumerate(row):
    #        np.insert(res, idx_row, idx_col)
    
    # OR...
    #res_time = np.array(results[2])

    res_rank =  [el[0] for el in results]
    res_cycle = [el[1] for el in results]
    res_time =  [el[2] for el in results]

    colorset = cm.rainbow
    norm = Normalize(vmin=0, vmax=10)

    plt.ylim(0.0, 0.00001)
    plt.xlim(-1.0, 20.0)
    plt.scatter(res_cycle, res_time, c=norm(res_rank), cmap=colorset)

    plt.xlabel('iteration')
    plt.ylabel('time (s)')
    plt.title('Lulesh Time per Iteration')
    plt.grid(True)
    plt.savefig("lulesh_time_per_iter.png")
    #plt.show()

    SOS.finalize();
    print "   ...DONE!"
    print 
    #############
  
if __name__ == "__main__":
    queryAndPlot()
    #############



