#!/usr/bin/env python
# file "example.py"
#
#   USAGE:   python ./example.py [loop_count]
#
#   See also:   ./trace_example.sh [loop_count]
#
#   Supported SOS.pack(name, type, value) types:
#           SOS.INT
#           SOS.LONG
#           SOS.DOUBLE
#           SOS.STRING
#

import sys
import time
import os
import pprint as pp
from ssos import SSOS

def demonstrateSOS():
    SOS = SSOS()

    sos_host = "localhost"
    sos_port = os.environ.get("SOS_CMD_PORT")

    print "Initializing SOS..."
    SOS.init()

    if (len(sys.argv) > 1):
        print "Packing, announcing, publishing..."
        SOS.pack("somevar", SOS.STRING, "Hello, SOS.  I'm a python!")
        SOS.announce()
        SOS.publish()

        count = int(0)
        count_max = int(sys.argv[1])

        print "   Packing " + sys.argv[1] + " integer values in a loop..."
        count = count + 1
        SOS.pack("loop_val", SOS.INT, count)
        SOS.announce()
        SOS.publish()

        while (count < count_max):
            count = count + 1
            SOS.pack("loop_val", SOS.INT, count)

        print "   Publishing the values..."
        SOS.publish()
        print "      ...OK!"

    #if (len(sys.argv) > 1):
    #    sql_string = "SELECT * FROM viewCombined WHERE frame = " + sys.argv[1] + ";"
    #else:
    
    sql_string = "SELECT * FROM viewCombined LIMIT 100000;"
 
    print "Sending this query to the SOS daemon: "
    print "    " + sql_string
    results, col_names = SOS.query(sql_string, sos_host, sos_port)
    print "Results:"
    print "    Output rows....: " + str(len(results))
    print "    Output values..: " + str(results)
    print "    Column count...: " + str(len(col_names)) 
    print "    Column names...: "# + str(col_names)    #pp.pprint(col_names)
    pp.pprint(col_names)
    print ""
    print "Finalizing..."
   
    SOS.finalize();
    print "   ...DONE!"
    print 

if __name__ == "__main__":
    demonstrateSOS()




