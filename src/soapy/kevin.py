import os
import sys
import sqlite3
import numpy as np
import pylab as pl
import time
import signal
from data_utils import is_outlier

conn = None

def open_connection(filename):
    global conn
    # check for file to exist
    print ("Checking for file: ", sqlite_file)
    while not os.path.exists(sqlite_file):
        print ("Waiting on file: ", sqlite_file)
        time.sleep(1)

    print("Connecting to: ", sqlite_file)
    # Connecting to the database file
    #conn = sqlite3.connect(sqlite_file)
    #fd = os.open(sqlite_file, os.O_RDONLY)
    #conn = sqlite3.connect('/dev/fd/%d' % fd)
    url = 'file:' + sqlite_file + '?mode=ro'
    #url = 'file:' + sqlite_file
    conn = sqlite3.connect(url, uri=True)
    conn.isolation_level=None
    c = conn.cursor()
    #c.execute('PRAGMA journal_mode=WAL;')
    #c.execute('PRAGMA synchronous   = ON;')
    #c.execute('PRAGMA cache_size    = 31250;')
    #c.execute('PRAGMA cache_spill   = FALSE;')
    #c.execute('PRAGMA temp_store    = MEMORY;')
    return c

def signal_handler(signal, frame):
    print("Detected ctrl-C...exiting.")
    print("Closing connection to database.")
    conn.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

def try_execute(c, statement, parameters=None):
    success = False
    while not success:
        try:
            if parameters:
                c.execute(statement,parameters);
            else:
                c.execute(statement);
            success = True
            break;
        except sqlite3.Error as e:
            print("database error...", e.args[0])

def get_ranks(c):
    sql_statement = ("select distinct comm_rank from tblpubs order by comm_rank;")
    #print("Executing query")
    try_execute(c,sql_statement);
    all_rows = c.fetchall()
    ranks = np.array([x[0] for x in all_rows])
    return ranks

def get_nodes(c):
    sql_statement = ("select distinct node_id from tblpubs order by comm_rank;")
    #print("Executing query")
    try_execute(c,sql_statement);
    all_rows = c.fetchall()
    nodes = np.array([x[0] for x in all_rows])
    return nodes

def do_chart(subplot, c, ranks, group_column, metric, plot_title, y_label, graph, axes):
    newplot = False
    if not graph:
        newplot = True
        graph = {}
    for r in ranks:
        sql_statement = ("SELECT tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblpubs.comm_rank FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE '" + metric + "') AND tblpubs." + group_column)
        if isinstance(r, str):
            sql_statement = (sql_statement + " like '" + r + "' order by tblvals.time_pack;")
        else:
            sql_statement = (sql_statement + " = " + str(r) + " order by tblvals.time_pack;")

        params = [metric,r]
        #print "Executing query: ", sql_statement, params
        #c.execute(sql_statement,params)
        try_execute(c, sql_statement)

        #print("Fetching rows.")
        all_rows = c.fetchall()
        if len(all_rows) <= 0:
            print("Error: query returned no rows.",)
            print(sql_statement, params)

        #print("Making numpy array of: metric_values")
        metric_values = np.array([x[2] for x in all_rows])
        #print("Making numpy array of: pack_time")
        pack_time = np.array([x[3] for x in all_rows])

        #print("len(pack_time) == ", len(pack_time))
        #print("len(metric_values) == ", len(metric_values))

        #print("Plotting: x=pack_time, y=metric_values")
        if newplot:
            axes = pl.subplot(subplot)
            axes.set_title(plot_title);
            graph[r] = (pl.plot(pack_time, metric_values, marker='*', linestyle='-', label=str(r))[0])
            axes.set_autoscale_on(True) # enable autoscale
            axes.autoscale_view(True,True,True)
            pl.legend(prop={'size':6})
            pl.ylabel(y_label)
            pl.xlabel("Timestamp")
        else:
            graph[r].set_data(pack_time, metric_values)
            axes.relim()        # Recalculate limits
            axes.autoscale_view(True,True,True) #Autoscale
    return graph,axes

def do_derived_chart(subplot, c, ranks, group_column, metric1, metric2, plot_title, y_label, graph, axes):
    newplot = False
    if not graph:
        newplot = True
        graph = {}
    for r in ranks:
        sql_statement = ("SELECT tblvals.row_id, tbldata.name, cast(tblvals.val as float), tblvals.time_pack FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE '" + metric1 + "') AND tblpubs." + group_column + " = " + str(r) + " order by tblvals.time_pack;")

        # print("Executing query 1")
        params = [metric1,r]
        try_execute(c,sql_statement);
        # print("Fetching rows.")
        all_rows1 = c.fetchall()
        if len(all_rows1) <= 0:
            print("Error: query returned no rows.",)
            print(sql_statement, params)

        sql_statement = ("SELECT tblvals.row_id, tbldata.name, cast(tblvals.val as float), tblvals.time_pack FROM tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid INNER JOIN tblpubs ON tblpubs.guid = tbldata.pub_guid WHERE tblvals.guid IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE '" + metric2 + "') AND tblpubs." + group_column + " = " + str(r) + " order by tblvals.time_pack LIMIT " + str(len(all_rows1)) + ";")
        # print("Executing query 2")
        params = [metric2,r]
        try_execute(c,sql_statement);
        # print("Fetching rows.")
        all_rows2 = c.fetchall()
        if len(all_rows2) <= 0:
            print("Error: query returned no rows.",)
            print(sql_statement, params)

        # print("Making numpy array of: metric_values")
        metric_values = np.array([x[2]/y[2] for x,y in zip(all_rows1,all_rows2)])
        # print(metric_values)
        # print("Making numpy array of: pack_time")
        pack_time = np.array([x[3] for x in all_rows1])
        # print(pack_time)
        if len(metric_values) > len(pack_time):
            np.resize(metric_values, len(pack_time))
        elif len(pack_time) > len(metric_values):
            np.resize(pack_time, len(metric_values))

        # print("len(pack_time) == ", len(pack_time))
        # print("len(metric_values) == ", len(metric_values))

        # print("Plotting: x=pack_time, y=metric_values")
        if newplot:
            axes = pl.subplot(subplot)
            axes.set_title(plot_title);
            graph[r] = (pl.plot(pack_time, metric_values, marker='*', linestyle='-', label=str(r))[0])
            axes.set_autoscale_on(True) # enable autoscale
            axes.autoscale_view(True,True,True)
            pl.legend(prop={'size':6})
            pl.ylabel(y_label)
            pl.xlabel("Timestamp")
        else:
            graph[r].set_data(pack_time, metric_values)
            axes.relim()        # Recalculate limits
            axes.autoscale_view(True,True,True) #Autoscale
        pl.draw()
    return graph,axes

# name of the sqlite database file
sqlite_file = sys.argv[1]

# open the connection
c = open_connection(sqlite_file)

# get the number of ranks
ranks = get_ranks(c)
while ranks.size == 0:
    time.sleep(1)
    ranks = get_ranks(c)
print ("ranks: ", ranks)
# get the number of nodes
nodes = get_nodes(c)
while nodes.size == 0:
    time.sleep(1)
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
graph1,axes1 = do_chart(321, c, nodes, "node_id", "%Mean::Node Power%","Mean Power","Node Power (Watts)", None,None)
graph2,axes2 = do_chart(322, c, nodes, "node_id", "%Mean::Node Energy%","Mean Energy","Node Energy (Joules)", None,None)
graph3,axes3 = do_chart(323, c, ranks, "comm_rank", "%Mean::Memory Footprint%","Mean memory footprint (KB)","Kilobytes", None,None)
graph4,axes4 = do_chart(324, c, [0], "comm_rank", "time","Lulesh time per iteration","Time", None,None)
graph5,axes5 = do_chart(325, c, [0], "comm_rank", "delta time","Lulesh delta time per iteration","Delta Time", None,None)
graph6,axes6 = do_derived_chart(326, c, ranks, "comm_rank", "%TAU::0::exclusive_TIME::MPI_Waitall()%","%TAU::0::calls::MPI_Waitall()%","MPI_Waitall() ","MPI_Waitall()", None, None)
print("Closing connection to database.")
# Closing the connection to the database file
conn.close()
pl.tight_layout()
while True:
    pl.pause(5.0)
    print("Updating chart...")
    # open the connection
    c = open_connection(sqlite_file)
    do_chart(321, c, nodes, "node_id", "%Mean::Node Power%","Mean Power","Node Power (Watts)", graph1, axes1)
    do_chart(322, c, nodes, "node_id", "%Mean::Node Energy%","Mean Energy","Node Energy (Joules)", graph2, axes2)
    do_chart(323, c, ranks, "comm_rank", "%Mean::Memory Footprint%","Mean memory footprint (KB)","Kilobytes", graph3, axes3)
    do_chart(324, c, [0], "comm_rank", "time","Lulesh time per iteration","Time", graph4, axes4)
    do_chart(325, c, [0], "comm_rank", "delta time","Lulesh delta time per iteration","Delta Time", graph5, axes5)
    do_derived_chart(326, c, ranks, "comm_rank", "%TAU::0::exclusive_TIME::MPI_Waitall()%","%TAU::0::calls::MPI_Waitall()%","MPI_Waitall() ","MPI_Waitall()", graph6, axes6)
    print("Closing connection to database.")
    # Closing the connection to the database file
    conn.close()


print("Done.")

