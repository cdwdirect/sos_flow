#!/usr/bin/python

import   sys
import   time
import   os
import   random
import   re
from     ssos   import SSOS

#####
#
def sosAutoTranspose():
    SOS = SSOS()

    sosHost = "localhost"
    sosPort = os.environ.get("SOS_CMD_PORT")

    printf("Initializing SOS: ...\b\b\b")
    SOS.init()
    printf("OK!\n")

    #####
    #
    # Get the maximum simulation cycle found in the database.
    #
    # NOTE: The cycleFieldName variable should match what is being used
    #       either by your application or SOSflow. If you are not using
    #       an explicit cycle value, you can use SOSflow's internal
    #       field named "frame" that is updated every time SOS_publish(...)
    #       is called. As long as you are publishing to SOS at the end
    #       of major program steps, this will give you what you want.
    #
    # NOTE: For online queries, if you want to ensure that your most
    #       current projection represents a complete set of values,
    #       and you're investigating a block-synchronous code, you can
    #       grab the current maximum and subtract one.
    #
    cycleFieldName = "frame"
    #
    sqlMaxFrame = "SELECT MAX(" + cycleFieldName + ") FROM viewCombined;"
    results, col_names = SOS.query(sqlMaxFrame, sosHost, sosPort)
    max_cycle = int(results[0][0])
    print "Maximum observed '" + cycleFieldName + "' value: " + str(max_cycle)
    #
    sqlMaxFrame = "SELECT MAX(comm_rank) FROM viewCombined;"
    results, col_names = SOS.query(sqlMaxFrame, sosHost, sosPort)
    rank_max = int(results[0][0])
    print "Maximum observed  'comm_rank' value: " + str(rank_max)
    #
    #####
    
    #####
    #
    # Get the list of field names we will use to build a custom query.
    #
    # NOTE: To filter out SOS_VAL_TYPE_STRING fields, add in:
    #            ... += "WHERE value_type != 3"
    sqlFieldNames = """
    SELECT
    DISTINCT value_name
    FROM viewCombined
    ;
    """
    results, col_names = SOS.query(sqlFieldNames, sosHost, sosPort)
    selectedFields = dict()
    selectedFields['name'] = [el[0] for el in results]
    name_count = len(selectedFields['name'])

    printf("(%d fields)", name_count)
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b")

    #
    # NOTE: Debug output...
    #
    #print "Selected " + str(name_count) + " unique names:"
    #for name in selectedFields['name']:
    #    print "    " + str(name)
    #print ""
    #
    #####

    #####
    #
    #  Compose a query with the unique numeric fields as columns:
    sqlValsToColsByRank  = """ """
    sqlValsToColsByRank += """ SELECT """
    sqlValsToColsByRank += """ comm_rank """
    for field_name in selectedFields['name']:
        sqlValsToColsByRank += """,GROUP_CONCAT( CASE WHEN """
        sqlValsToColsByRank += ' value_name LIKE "' + field_name + '" '
        sqlValsToColsByRank += ' THEN value END) AS "' + field_name + '" '
    #end:for field_name
    #
    # NOTE: We can now manually grab some hardcoded field names
    #       that might not have been included in selectedFields
    #       if things were being filtered by type:
    #
    #
    sqlValsToColsByRank += """ FROM viewCombined """
    #
    #  NOTE: Uncomment this, and comment out the 'GROUP BY' frame below,
    #        for cases where we only want to see the largest frame.
    #
    #sqlValsToColsByRank += " WHERE frame = " + str(simCycle) + " " 
    sqlValsToColsByRank += """ GROUP BY """
    sqlValsToColsByRank += """ comm_rank """
    sqlValsToColsByRank += """,frame """
    sqlValsToColsByRank += """;"""
    #
    results, col_names = SOS.query(sqlValsToColsByRank, sosHost, sosPort)
    #
    #
    #####
    print str(col_names)
    print str(results)
    #####
    #
    # Whew!  All done!
    #
    # NOTE: See vtkWriter.py for more details.
    #
    SOS.finalize();
    #
    #####
    print "   ...DONE!"
    print 
    return



def printf(format, *args):
    sys.stdout.write(format % args)

###############################################################################
###############################################################################

if __name__ == "__main__":
    sosAutoTranspose()
    #end:FILE


