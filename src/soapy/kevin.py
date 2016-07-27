import os
import sys
import sqlite3
import numpy as np
import pylab as pl

from data_utils import is_outlier

def do_chart(subplot, c, metric, plot_title, x_label, graph):
    sql_statement = ("SELECT tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE ?) AND tblpubs.comm_rank = 0 order by tblvals.time_pack;")

    print("Executing query")
    c.execute(sql_statement,metric);

    print("Fetching rows.")
    all_rows = c.fetchall()
    names = np.array([x[1] for x in all_rows])

    print("Making numpy array of: metric_values")
    metric_values = np.array([x[2] for x in all_rows])
    print("Making numpy array of: pack_time")
    pack_time = np.array([x[3] for x in all_rows])

    print("len(pack_time) == ", len(pack_time))
    print("len(metric_values) == ", len(metric_values))

    print("Skipping outlier-filter stage.")

    print("Plotting: x=pack_time, y=metric_values")
    pl.subplot(subplot)
    pl.title(plot_title);
    if (graph == None):
        graph = pl.plot(pack_time, metric_values)[0]
        print type(graph)
    else:
        graph.set_data(pack_time, metric_values)
    pl.ylabel(x_label)
    pl.xlabel("Timestamp")
    print("Showing plot...")
    pl.draw()
    return graph

def do_derived_chart(subplot, c, metric1, metric2, plot_title, x_label, graph):
    sql_statement = ("SELECT tblvals.row_id, tbldata.name, cast(tblvals.val as float), tblvals.time_pack FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE ?) AND tblpubs.comm_rank = 0 order by tblvals.time_pack;")

    print("Executing query 1")
    c.execute(sql_statement,metric1);
    print("Fetching rows.")
    all_rows1 = c.fetchall()

    print("Executing query 2")
    c.execute(sql_statement,metric2);
    print("Fetching rows.")
    all_rows2 = c.fetchall()

    print("Making numpy array of: metric_values")
    metric_values = np.array([x[2]/y[2] for x,y in zip(all_rows1,all_rows2)])
    print(metric_values)
    print("Making numpy array of: pack_time")
    pack_time = np.array([x[3] for x in all_rows1])
    print(pack_time)

    print("len(pack_time) == ", len(pack_time))
    print("len(metric_values) == ", len(metric_values))

    print("Skipping outlier-filter stage.")

    print("Plotting: x=pack_time, y=metric_values")
    pl.subplot(subplot)
    pl.title(plot_title);
    if (graph == None):
        graph = pl.plot(pack_time, metric_values)[0]
    else:
        graph.set_data(pack_time, metric_values)
    pl.ylabel(x_label)
    pl.xlabel("Timestamp")
    print("Showing plot...")

# name of the sqlite database file
sqlite_file = sys.argv[1]

print("Connecting to: ", sqlite_file)
# Connecting to the database file
conn = sqlite3.connect(sqlite_file)
c = conn.cursor()

#resize the figure
# Get current size
fig_size = pl.rcParams["figure.figsize"]
# Prints: [8.0, 6.0]
print "Current size:", fig_size
# Set figure width to 12 and height to 9
fig_size[0] = 12
fig_size[1] = 9
pl.rcParams["figure.figsize"] = fig_size
pl.ion()
# rows, columns, figure number for subplot value
graph1 = do_chart(321, c,["%Mean::Node Power%"],"Mean Power","Node Power (Watts)", None)
graph2 = do_chart(322, c,["%Mean::Node Energy%"],"Mean Energy","Node Energy (Joules)", None)
graph3 = do_chart(323, c,["%Mean::Memory Footprint%"],"Mean memory footprint (KB)","Kilobytes", None)
graph4 = do_chart(324, c,["time"],"Lulesh time per iteration","Time", None)
graph5 = do_chart(325, c,["delta time"],"Lulesh delta time per iteration","Delta Time", None)
graph6 = do_derived_chart(326, c,["%TAU::0::exclusive_TIME::MPI_Waitall()%"],["%TAU::0::calls::MPI_Waitall()%"],"MPI_Waitall() ","MPI_Waitall()", None)
pl.tight_layout()
pl.draw()
while True:
    pl.pause(1.0)
    do_chart(321, c,["%Mean::Node Power%"],"Mean Power","Node Power (Watts)", graph1)
    do_chart(322, c,["%Mean::Node Energy%"],"Mean Energy","Node Energy (Joules)", graph2)
    do_chart(323, c,["%Mean::Memory Footprint%"],"Mean memory footprint (KB)","Kilobytes", graph3)
    do_chart(324, c,["time"],"Lulesh time per iteration","Time", graph4)
    do_chart(325, c,["delta time"],"Lulesh delta time per iteration","Delta Time", graph5)
    do_derived_chart(326, c,["%TAU::0::exclusive_TIME::MPI_Waitall()%"],["%TAU::0::calls::MPI_Waitall()%"],"MPI_Waitall() ","MPI_Waitall()", graph6)
    pl.draw()

print("Closing connection to database.")
# Closing the connection to the database file
conn.close()

print("Done.")

