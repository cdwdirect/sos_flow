import os
import sys
import sqlite3
import numpy as np
import pylab as pl

from data_utils import is_outlier


# name of the sqlite database file
sqlite_file = os.environ.get("SOS_LOCATION", ".") + "/" + sys.argv[1]
table_name = 'tblvals'   # name of the table to be queried

print("Connecting to: ", sqlite_file)
# Connecting to the database file
conn = sqlite3.connect(sqlite_file)
c = conn.cursor()


#sql_statement = (" SELECT "
#                 "   tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblvals.time_send, tblvals.time_recv "
#                 " FROM "
#                 "   (tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid) "
#                 " WHERE "
#                 "   tblvals.guid "
#                 "      IN (SELECT guid FROM tbldata WHERE tbldata.name LIKE 'cpu_avg%') "
#                 " AND "
#                 "   tblvals.rowid < 100; ")

sql_statement = (" SELECT "
                 "   tblvals.row_id, tbldata.name, tblvals.val, tblvals.time_pack, tblvals.time_send, tblvals.time_recv "
                 " FROM "
                 "   (tblvals INNER JOIN tbldata ON tblvals.guid = tbldata.guid) "
                 ";")

                 #" WHERE "
                 #"   tblvals.row_id > 10000000 "
                 #" AND "
                 #"   tblvals.row_id < 20000000;")

print("Executing query: ", sql_statement)
c.execute(sql_statement);

print("Fetching rows.")
all_rows = c.fetchall()

print("Making numpy array of: pack_time")
pack_time = np.array([x[3] for x in all_rows])
print("Making numpy array of: latencies")
latencies = np.array([(x[5] - x[4]) for x in all_rows])

print("len(pack_time) == ", len(pack_time))
print("len(latencies) == ", len(latencies))

print("Skipping outlier-filter stage.")
#filtered_pack_time = pack_time[~is_outlier(latencies)]
#filtered_latencies = latencies[~is_outlier(latencies)]

print("Plotting: x=pack_time, y=latencies")
pl.title("Latency Between Client SOS_publish() and Daemon DB Insert");
pl.plot(pack_time, latencies)
pl.ylabel("Latency (sec.)")
pl.xlabel("Timestamp When Client Sent Value to Daemon (sec. from earliest value)")
print("Showing plot...")
pl.show()

print("Closing connection to database.")
# Closing the connection to the database file
conn.close()

print("Done.")

# ----------
# Reference:
#
# Make an array of x values

#x = [1, 2, 3, 4, 5]

# Make an array of y values for each x value

#y = [1, 4, 9, 16, 25]

# use pylab to plot x and y
#pl.plot(x, y)
# show the plot on the screen
#pl.show()



#end

