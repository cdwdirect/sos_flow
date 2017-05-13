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
import pprint as pp
from ssos import SSOS

def demonstrateSOS():
    SOS = SSOS()

    print "Initializing SOS..."
    SOS.init()

#    print "Packing, announcing, publishing..."
#    SOS.pack("somevar", SOS.STRING, "Hello, SOS.  I'm a python!")
#    SOS.announce()
#    SOS.publish()

    if (len(sys.argv) > 1):
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

    sql_string = "SELECT * FROM tblVals WHERE rowid < 600;"
    print "Sending this query to the SOS daemon: " + sql_string
    results, col_names = SOS.query(sql_string)
    print "Column count...: " + str(len(col_names)) 
    print "Column names...: " + str(col_names)    #pp.pprint(col_names)
    print "Output rows....: " + str(len(results))    #pp.pprint(results)

    print "Finalizing..."
    SOS.finalize();
    print ""
    print "   ...DONE!"
    print ""


if __name__ == "__main__":
    demonstrateSOS()




