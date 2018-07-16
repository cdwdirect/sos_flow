#!/usr/bin/env python
# file "grab.py"
#
#   USAGE:   python ./grab.py
#
#   Pull the pub manifest from the daemon and display it.

import sys
import time
import os
import pprint as pp
from ssos import SSOS

def demonstrateManifest():
    SOS = SSOS()

    sos_host = "localhost"
    sos_port = os.environ.get("SOS_CMD_PORT")

    SOS.init()
    
    max_frame, manifest, col_names = SOS.request_pub_manifest("", sos_host, sos_port)
    
    print "Manifest:"
    print (str(col_names)) 
    
    # Print out the manifest in a pretty column-aligned way:
    widths = [max(map(len, col)) for col in zip(*manifest)]
    for row in manifest: 
        print "  ".join((val.ljust(width) for val, width in zip(row, widths)))
    
    print ""
    print "    Pub count .....: " + str(len(manifest)) 
    print "    Max frame .....: " + str(max_frame)
    print ""
    SOS.finalize();
    print 

if __name__ == "__main__":
    demonstrateManifest()




