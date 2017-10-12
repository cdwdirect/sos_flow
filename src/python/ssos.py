# file "ssos.py"
#
#   SSOS API for Python
#

import time
import sys
import subprocess
import os
import glob
import io
import csv
from ssos_python import ffi, lib

class SSOS:
    INT    = lib.SSOS_TYPE_INT 
    LONG   = lib.SSOS_TYPE_LONG
    DOUBLE = lib.SSOS_TYPE_DOUBLE
    STRING = lib.SSOS_TYPE_STRING

    def init(self):
        prog_name = ffi.new("char[]", sys.argv[0])
        lib.SSOS_init(prog_name)
        is_online_flag = ffi.new("int*")
        is_online_flag[0] = 0
        lib.SSOS_is_online(is_online_flag)
        connect_delay = 0
        while((connect_delay < 4) and (is_online_flag[0] < 1)):
            lib.SSOS_is_online(is_online_flag)
            print "   ... waiting to connect to SOS"
            time.sleep(0.5)
            connect_delay += 1
        
        if (is_online_flag[0] < 1):
            print "ERROR: Unable to connect to the SOS daemon."
            exit()

    def is_online(self):
        is_online_flag = ffi.new("int*")
        is_online_flag[0] = 0
        lib.SSOS_is_online(is_online_flag)
        return bool(is_online_flag[0])

    def is_query_done(self):
        is_query_done_flag = ffi.new("int*")
        is_query_done_flag[0] = 0
        lib.SSOS_is_query_done(is_query_done_flag)
        return bool(is_query_done_flag[0])

    def pack(self, pyentry_name, entry_type, pyentry_value):
        entry_name = ffi.new("char[]", pyentry_name)
        if (entry_type == self.INT):
            entry_addr = ffi.new("int*", pyentry_value)
        elif (entry_type == self.LONG):
            entry_addr = ffi.new("long*", pyentry_value)
        elif (entry_type == self.DOUBLE):
            entry_addr = ffi.new("double*", pyentry_value)
        elif (entry_type == self.STRING):
            entry_addr = ffi.new("char[]", pyentry_value)
        else:
            print "invalid type provided to SOS.pack(...): " \
                + entry_type + "  (doing nothing)"
            return
        lib.SSOS_pack(entry_name, entry_type, entry_addr)

    def query(self, sql, host, port):
        res_sql = ffi.new("char[]", sql)
        res_obj = ffi.new("SSOS_query_results*")
        res_host = ffi.new("char[]", host)
        res_port = ffi.new("int*", int(port))
        lib.SSOS_query_exec_blocking(res_sql, res_obj, res_host, res_port[0])
        
        results = [] 
        for row in range(res_obj.row_count):
            thisrow = []
            for col in range(res_obj.col_count):
                thisrow.append(ffi.string(res_obj.data[row][col]))
            #print "results[{}] = {}".format(row, thisrow)
            results.append(thisrow)

        # Generate the column name list:
        col_names = []
        for col in range(0, res_obj.col_count):
           col_names.append(ffi.string(res_obj.col_names[col]))

        lib.SSOS_results_destroy(res_obj)
        return (results, col_names)


    def queryAllAggregators(self, sql)
        sosPort = os.getenv("SOS_CMD_PORT")
        sosKeyPath = os.getenv("SOS_EVPATH_MEETUP")
        sosKeyFiles = glob.glob(sosKeyPath + "/*.key")
        
        if len(sosKeyFiles) == 0:
            print "ERROR: No .key files found in " + sosKeyPath
            print "       Query will not be run."
            return ([[]], [])

        for key in sosKeyFiles:


        # Wait for all results:
        # Append all results together:

        return (results, col_names) 

#    def queryFile(self, sql, filename):
#        tmpresultfile = str(os.getpid()) + ".tmpsql.csv"
#        rc = subprocess.call("sqlite3", "-header -csv -column \"" + sql \
#                + "\" > " + tmpresultfile)
#        results = []
#        col_names = []
#        if rc != 0:
#            print "Error executing query: \" + sql + "\""
#            return (results, col_names)
#        with io.open('times.csv', 'r', newline='') as csvfile:
#            data = csv.reader(csvfile)
#            row_count = len(data)
#            for row in range(2, row_count)
#                this_row = []
#                col_count = len(row)
#                for col in range(col_count)
#                    this_row.append(data[row][col])
#                results.append(this_row)
#            for col in range(col_count)
#                col_names.append(data[1][col]
#        return (results, col_names)
#
#
#        for time_iter, start_time, start_sec, stop_time, stop_sec in data:
#            print time_iter + ' : ' + str(int(stop_sec) - int(start_sec))
  



    def trigger(self, handle, payload_size, payload_data):
        c_handle = ffi.new("char[]", handle)
        c_payload_size = ffi.new("int*", payload_size)
        c_payload_data = ffi.new("unsigned char[]", payload_data)
        lib.SSOS_sense_trigger(c_handle, c_payload_size[0], c_payload_data)

    def announce(self):
        lib.SSOS_announce()

    def publish(self):
        lib.SSOS_publish()

    def finalize(self):
        lib.SSOS_finalize()

if (__name__ == "__main__"):
    print "This a library wrapper intended for use in other Python scripts."
