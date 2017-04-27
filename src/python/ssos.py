#
#   SSOS API for Python
#

import time
import sys
from ssos_python import ffi, lib

class SSOS:
    INT    = lib.SSOS_TYPE_INT 
    LONG   = lib.SSOS_TYPE_LONG
    DOUBLE = lib.SSOS_TYPE_DOUBLE
    STRING = lib.SSOS_TYPE_STRING

    def init(self):
        lib.SSOS_init()
        is_online_flag = ffi.new("int*")
        is_online_flag[0] = 0
        lib.SSOS_is_online(is_online_flag)
        while(is_online_flag[0] < 1):
            lib.SSOS_is_online(is_online_flag)
            print "   ... attempting to connect to SOS"
            time.sleep(0.5)

    def is_online(self):
        is_online_flag = ffi.new("int*")
        is_online_flag[0] = 0
        lib.SSOS_is_online(is_online_flag)
        return bool(is_online_flag[0])
 
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

    def query(self, sql):
        res_sql = ffi.new("char[]", sql)
        res_obj = ffi.new("SSOS_query_results*")
        lib.SSOS_exec_query(res_sql, res_obj)
        print "{}{}".format("ssos.py: res_obj.col_count == ", res_obj.col_count)
        print "{}{}".format("ssos.py: res_obj.row_count == ", res_obj.row_count)

        results = [[]] 
        for row in range(res_obj.row_count):
            thisrow = []
            for col in range(res_obj.col_count):
                thisrow.append(ffi.string(res_obj.data[row][col]))
            print "results[{}] = {}".format(row, thisrow)
            results.append(thisrow)


       # Generate the column name list:
        col_names = []
        for col in range(0, (res_obj.col_count)):
           col_names.append(ffi.string(res_obj.col_names[col]))

        return (results, col_names)

    def announce(self):
        lib.SSOS_announce()

    def publish(self):
        lib.SSOS_publish()

    def finalize(self):
        lib.SSOS_finalize()

if (__name__ == "__main__"):
    print "This a library intended for use in other Python scripts."
