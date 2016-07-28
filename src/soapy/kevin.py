import os
import sys
import sqlite3
import numpy as np
import pylab as pl
import time

from data_utils import is_outlier

def get_ranks(c):
    sql_statement = ("select distinct comm_rank from tblpubs order by comm_rank;")
    #print("Executing query")
    c.execute(sql_statement);
    all_rows = c.fetchall()
    ranks = np.array([x[0] for x in all_rows])
    return ranks

def get_nodes(c):
    sql_statement = ("select distinct node_id from tblpubs order by comm_rank;")
    #print("Executing query")
    c.execute(sql_statement);
    all_rows = c.fetchall()
    nodes = np.array([x[0] for x in all_rows])
    return nodes

def do_chart(subplot, c, ranks, group_column, metric, plot_title, y_label, graph):
    newplot = False
    if not graph:
        newplot = True
        graph = {}
    for r in ranks:
        sql_statement = ("SELECT tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblpubs.comm_rank FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE ?) AND tblpubs." + group_column + " like ? order by tblvals.time_pack;")

        params = [metric,r]
        #print "Executing query: ", sql_statement, params
        c.execute(sql_statement,params)

        #print("Fetching rows.")
        all_rows = c.fetchall()
        names = np.array([x[1] for x in all_rows])

        #print("Making numpy array of: metric_values")
        metric_values = np.array([x[2] for x in all_rows])
        #print("Making numpy array of: pack_time")
        pack_time = np.array([x[3] for x in all_rows])

        #print("len(pack_time) == ", len(pack_time))
        #print("len(metric_values) == ", len(metric_values))

        #print("Plotting: x=pack_time, y=metric_values")
        pl.subplot(subplot)
        pl.title(plot_title);
        if newplot:
            graph[r] = (pl.plot(pack_time, metric_values)[0])
        else:
            graph[r].set_data(pack_time, metric_values)
        pl.ylabel(y_label)
        pl.xlabel("Timestamp")
    return graph

def do_derived_chart(subplot, c, ranks, group_column, metric1, metric2, plot_title, y_label, graph):
    newplot = False
    if not graph:
        newplot = True
        graph = {}
    for r in ranks:
        sql_statement = ("SELECT tblvals.row_id, tbldata.name, cast(tblvals.val as float), tblvals.time_pack FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE ?) AND tblpubs." + group_column + " = ? order by tblvals.time_pack;")

        # print("Executing query 1")
        params = [metric1,r]
        c.execute(sql_statement,params);
        # print("Fetching rows.")
        all_rows1 = c.fetchall()

        # print("Executing query 2")
        params = [metric2,r]
        c.execute(sql_statement,params);
        # print("Fetching rows.")
        all_rows2 = c.fetchall()

        # print("Making numpy array of: metric_values")
        metric_values = np.array([x[2]/y[2] for x,y in zip(all_rows1,all_rows2)])
        # print(metric_values)
        # print("Making numpy array of: pack_time")
        pack_time = np.array([x[3] for x in all_rows1])
        # print(pack_time)

        # print("len(pack_time) == ", len(pack_time))
        # print("len(metric_values) == ", len(metric_values))

        # print("Plotting: x=pack_time, y=metric_values")
        pl.subplot(subplot)
        pl.title(plot_title);
        if newplot:
            graph[r] = (pl.plot(pack_time, metric_values)[0])
        else:
            graph[r].set_data(pack_time, metric_values)
        pl.ylabel(y_label)
        pl.xlabel("Timestamp")

# name of the sqlite database file
sqlite_file = sys.argv[1]

# check for file to exist
print ("Checking for file: ", sqlite_file)
while not os.path.exists(sqlite_file):
    print ("Waiting on file: ", sqlite_file)
    time.sleep(1)
# wait just a bit more
time.sleep(2)

print("Connecting to: ", sqlite_file)
# Connecting to the database file
conn = sqlite3.connect(sqlite_file)
#fd = os.open(sqlite_file, os.O_RDONLY)
#conn = sqlite3.connect('/dev/fd/%d' % fd)
#url = 'file:' + sqlite_file + '?mode=ro'
#conn = sqlite3.connect(url, uri=True)
c = conn.cursor()
c.execute('PRAGMA journal_mode=WAL;')

# get the number of ranks
ranks = get_ranks(c)
print ("ranks: ", ranks)
# get the number of ranks
nodes = get_nodes(c)
print ("nodes: ", nodes)
#resize the figure
# Get current size
fig_size = pl.rcParams["figure.figsize"]
# Set figure width to 12 and height to 9
fig_size[0] = 12
fig_size[1] = 9
pl.rcParams["figure.figsize"] = fig_size
pl.ion()
# rows, columns, figure number for subplot value
graph1 = do_chart(321, c, nodes, "node_id", "%Mean::Node Power%","Mean Power","Node Power (Watts)", None)
graph2 = do_chart(322, c, nodes, "node_id", "%Mean::Node Energy%","Mean Energy","Node Energy (Joules)", None)
graph3 = do_chart(323, c, ranks, "comm_rank", "%Mean::Memory Footprint%","Mean memory footprint (KB)","Kilobytes", None)
graph4 = do_chart(324, c, [0], "comm_rank", "time","Lulesh time per iteration","Time", None)
graph5 = do_chart(325, c, [0], "comm_rank", "delta time","Lulesh delta time per iteration","Delta Time", None)
graph6 = do_derived_chart(326, c, ranks, "comm_rank", "%TAU::0::exclusive_TIME::MPI_Waitall()%","%TAU::0::calls::MPI_Waitall()%","MPI_Waitall() ","MPI_Waitall()", None)
pl.tight_layout()
pl.draw()
while True:
    pl.pause(1.0)
    print("Updating chart...")
    do_chart(321, c, nodes, "node_id", "%Mean::Node Power%","Mean Power","Node Power (Watts)", graph1)
    do_chart(322, c, nodes, "node_id", "%Mean::Node Energy%","Mean Energy","Node Energy (Joules)", graph2)
    do_chart(323, c, ranks, "comm_rank", "%Mean::Memory Footprint%","Mean memory footprint (KB)","Kilobytes", graph3)
    do_chart(324, c, [0], "comm_rank", "time","Lulesh time per iteration","Time", graph4)
    do_chart(325, c, [0], "comm_rank", "delta time","Lulesh delta time per iteration","Delta Time", graph5)
    do_derived_chart(326, c, ranks, "comm_rank", "%TAU::0::exclusive_TIME::MPI_Waitall()%","%TAU::0::calls::MPI_Waitall()%","MPI_Waitall() ","MPI_Waitall()", graph6)
    pl.draw()

print("Closing connection to database.")
# Closing the connection to the database file
conn.close()

print("Done.")

