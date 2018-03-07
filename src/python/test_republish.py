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

    print "Initializing SOS..."
    SOS.init()
    
    count = 0
    count_max = 10
    print "    Packing " + str(count_max) + " integer values in a loop..."
    while (count < count_max):
        count = count + 1
        SOS.pack(("loop_val_" + str(count)), SOS.INT, count)
        #SOS.announce()
        SOS.publish()

 
    sql_pubs = "SELECT * FROM tblPubs;"
    sql_data = "SELECT * FROM tblData;"

    pubs, col_names = SOS.query(sql_pubs, "localhost", os.environ.get("SOS_CMD_PORT"))

    print "-----"
    print "Pubs: (" + str(len(pubs)) + ")"
    count = 0
    print str(col_names)
    while count < len(pubs):
        print str(pubs[count])
        count = count + 1

    data, col_names = SOS.query(sql_data, "localhost", os.environ.get("SOS_CMD_PORT"))

    print "-----"
    print "Data: (" + str(len(data)) + ")"
    count = 0
    print str(col_names)
    while count < len(data):
        print str(data[count])
        count = count + 1

    print ""
    
    SOS.finalize();
    print "   ...DONE!"
    print 

if __name__ == "__main__":
    demonstrateSOS()




