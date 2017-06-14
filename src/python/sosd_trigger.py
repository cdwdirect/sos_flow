#!/usr/bin/env python
# file "sosd_trigger.py"
#
#   USAGE:   python ./sosd_trigger.py
#

import sys
import time
import pprint as pp
from ssos import SSOS

def triggerSOSD():
    SOS = SSOS()

    sense_handle = "example_sense"
    payload_data = "Hello, you've been triggered by Python!"
    payload_size = len(payload_data)

    print "Initializing SOS..."
    SOS.init()
    print "Triggering SOSD w/the following:"
    print "   sense_handle = " + str(sense_handle)
    print "   payload_size = " + str(payload_size)
    print "   payload_data = " + str(payload_data)
    SOS.trigger(sense_handle, payload_size, payload_data)
    SOS.finalize();
    print "DONE!"
    print 

if __name__ == "__main__":
    triggerSOSD()




