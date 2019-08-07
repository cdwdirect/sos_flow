#!/usr/bin/python

#####
#
# NOTE: Keep any useful paths to custom Pythons here, with a note.
#
# VPA17 Alpine stack:
# /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python
#
#####

import   sys
import   subprocess
import   time
import   os
import   random
import   re
import   vtk_writer
import   numpy  as  np
from     ssos   import SSOS

#
# NOTE: This script is intended to be run standalone, and to serve as an
#       example of how to interact with the SOSflow runtime to extract
#       geometry. You may need to update some hardcoded field names that
#       were used in prior experiments for this to work for you.
#
#       Additional support functions can be found below this main function.

#####
#
def sosVTKProjector():
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
    # Here we drive the generation of the .vtk file[s]:
    filenames = []
    #
    # NOTE: When allocation time is scarce, 'stride' here can be
    #       set so that intermediate cycles can be skipped, which is
    #       especially useful when there are thousands of cycles.
    #
    stride = 1
    #
    # EXAMPLE A: Generate .vtk set for ALL simulation cycles:
    print "Generating VTK files..."
    lastX = [0.0]*(rank_max + 1)
    lastY = [0.0]*(rank_max + 1)
    lastZ = [0.0]*(rank_max + 1)
    for simCycle in range(0, max_cycle, stride):
        printf("    ... %d of %d ", (simCycle + 1), max_cycle)
        vtkOutputFileName = generateVTKFile(SOS, cycleFieldName, simCycle, lastX, lastY, lastZ)
        filenames.append(vtkOutputFileName)
    #end:for simCycle
    printf("                                        ")
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b")
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b")
    # -----
    # EXAMPLE B: Generate .vtk file for MOST RECENT cycle:
    #vtkOutputFile = generateVTKFile(selectedFields, max_cycle)
    #filenames.append(vtkOutputFile)
    #
    #####

    #####
    #
    # Produce a dataset.visit 'group file' that tells VisIt obout our per-
    # cycle .vtk files, to explore them in sequence:
    vtk_writer.write_visit_file(filenames)
    #
    #####
 
    #####
    #
    # NOTE: This block of code can be used to launch VisIt automatically
    #       after the script generates the input file.
    #
    #visit.AddArgument("-par")
    #visit.Launch()
    #OpenDatabase("dataset.visit")
    #AddPlot("Pseudocolor", "rank")
    #AddPlot("Mesh", "mesh")
    #DrawPlots()
    # loop through times
    #tsNames = GetWindowInformation().timeSliders
    #for ts in tsNames:
    #    SetActiveTimeSlider(ts)
    #for state in list(range(TimeSliderGetNStates()))[::10] + [0]:
    #    SetTimeSliderState(state)
    #print "Setting share_power permissions on the newly created VTK files..."
    #subprocess.call("$PROJECT_BASE/share_power .", shell=True)
    #print ""
    #print "Sleeping for 100 seconds..."
    #print ""
    #time.sleep(100)
    #
    #####

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
#
#end:def sosVTKProjector()

#####
#
def generateVTKFile(SOS, cycleFieldName, simCycle, lastX, lastY, lastZ):
    sosHost = "localhost"
    sosPort = os.environ.get("SOS_CMD_PORT")
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
    WHERE """ + cycleFieldName + """ = """ + str(simCycle) + """
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


    # NOTE: VisIt will give errors when you are projecting a field that is
    #       not present in all of its data sets. If that were going to be
    #       a problem, i.e. for production code, it might be good to filter
    #       the list of fields to only those present in ALL data sets, and
    #       provide the user with a list of fields that were not included
    #       (for this reason) ... or include fields but with a special
    #       value for 'not present'.
    

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
    sqlValsToColsByRank += """, GROUP_CONCAT( CASE WHEN """
    sqlValsToColsByRank += ' value_name LIKE "%CRAY_PMI_X%" '
    sqlValsToColsByRank += ' THEN value END) AS "PMI_X" '
    sqlValsToColsByRank += """, GROUP_CONCAT( CASE WHEN """
    sqlValsToColsByRank += ' value_name LIKE "%CRAY_PMI_Y%" '
    sqlValsToColsByRank += ' THEN value END) AS "PMI_Y" '
    sqlValsToColsByRank += """, GROUP_CONCAT( CASE WHEN """
    sqlValsToColsByRank += ' value_name LIKE "%CRAY_PMI_Z%" '
    sqlValsToColsByRank += ' THEN value END) AS "PMI_Z" '
    #
    sqlValsToColsByRank += """ FROM viewCombined """
    sqlValsToColsByRank += " WHERE frame = " + str(simCycle) + " " 
    sqlValsToColsByRank += """ GROUP BY """
    sqlValsToColsByRank += """ comm_rank """
    #
    # NOTE: Redacted, since we're only running this for one cycle.
    #
    # sqlValsToColsByRank += """,frame """
    sqlValsToColsByRank += """;"""
    #
    results, col_names = SOS.query(sqlValsToColsByRank, sosHost, sosPort)
    #
    #
    #####

    #####
    #
    # Build an attribute dictionary of the results
    #
    # NOTE: These keys use the CLEANED name!  (See below)
    #
    keyX = 'TAU_0_Metadata_CRAY_PMI_X'
    keyY = 'TAU_0_Metadata_CRAY_PMI_Y'
    keyZ = 'TAU_0_Metadata_CRAY_PMI_Z'
    #
    #
    attr = dict()
    attr['comm_rank']  =  [el[0] for el in results]
    fieldNames = list()
    position = 1
    for field_name in selectedFields['name']:
        # NOTE: Replace non-alphanumerics in the metric field names
        #       with '_' per sequence of them, so VisIt doesn't hang:
        cleanFieldName = re.sub('[^0-9a-zA-Z]+', '_', str(field_name))
        fieldNames.append(cleanFieldName)
        attr[cleanFieldName] = [el[position] for el in results]
        position += 1
    #end:for field_name

    #for field_name in selectedFields['name']:
    #    rank = 0
    #    for this_ranks_value in attr[field_name]:
    #        print "comm_rank(" + str(rank) + ")." + field_name + " = " + this_ranks_value
    #        rank += 1
    #


    #
    #####

    #####
    #
    # Use a function to generate hexahedral coordinates regionally grouped
    # on or around an arbitrary X, Y, and Z.
    #
    # This is important to do when multiple ranks or threads need to be
    # distinguishable in a visualization but have been encoded into SOSflow
    # with the identical geometry.  It can also be useful when point-cloud
    # data is to be displayed within a volume rendering.
    #
    # NOTE: If you have full hexahedral geometry, encoded as eight tuples of
    #       x, y, and z floating point values in a sequence, all seperated
    #       by spaces, you can comment out the below block and simply
    #       append the strings in rank order to the finalHexStrings list.
    #
    # NOTE: The CRAY_PMI_X/Y/Z fieldnames shown below are hardcoded for
    #       a particular experiment and should be replaced with your X, Y,
    #       and Z field names if they are different.
    #
    finalHexStrings = list()
    plotState = dict()
    rank_max = len(attr['comm_rank'])
    #
    #
    for rank in range(rank_max):
        ctrX = 0.0
        ctrY = 0.0
        ctrZ = 0.0
        # Sanity checking on the presence of values makes these
        # ugly code blocks...
        #---
        if keyX in attr:
            if attr[keyX][rank] == "NULL":
                ctrX = lastX[rank]
            else:
                ctrX = float(attr[keyX][rank])
        else:
            ctrX = lastX[rank]
        #---
        if keyY in attr:
            if attr[keyY][rank] == "NULL":
                ctrY = lastY[rank]
            else:
                ctrY = float(attr[keyY][rank])
        else:
            ctrY = lastY[rank]
        #---
        if keyZ in attr:
            if attr[keyZ][rank] == "NULL":
                ctrZ = lastZ[rank]
            else:
                ctrZ = float(attr[keyZ][rank])
        else:
            ctrZ = lastZ[rank]
        #---
        # Update the last known good values, if we have new ones. 
        if ctrX != lastX[rank]:
            lastX[rank] = ctrX
        if ctrY != lastY[rank]:
            lastY[rank] = ctrY
        if ctrZ != lastZ[rank]:
            lastZ[rank] = ctrZ
        #
        #
        #rankGeometry = xyzToHexStringRandomScatter(ctrX, ctrY, ctrZ, plotState)
        rankGeometry = xyzToHexStringStackedWafers(ctrX, ctrY, ctrZ, plotState)
        #
        finalHexStrings.append(rankGeometry)
        #
    #end:for rank
    #
    #####

    #####
    #
    # Lay out the data the way the vtkWriter class expects it:
    dset = vtk_writer.vtk_hex_data_set()
    dset.clear()
    dset.set_cycle(simCycle)
    #
    for rank in range(0, rank_max - 1):
        vtkHexAttributes = {}
        for cleanName in fieldNames:
            vtkHexAttributes[cleanName] = attr[cleanName][rank]
        #end:for cleanName
        #
        # NOTE: Ensure there is 'at least' this value available:
        #
        vtkHexAttributes["rank"] = rank
        #
        hex_coords = [None]*24
        hex_coords = finalHexStrings[rank].split()
        dset.add_hex(hex_coords, vtkHexAttributes, rank)
    #end:for rank
    #
    outputFileName = dset.write_vtk_file()
    return outputFileName
#
#end:def generateVTKFile(...)

    

# NOTE: Accessory functions:



def xyzToHexStringStackedWafers(ctrX, ctrY, ctrZ, elev):
    #         1*______*2 (+)
    #         /|     /|
    #        / |    / |
    #      3*------*4 |
    #       | 5*___|__*6
    #       | /    | /
    #       |/     |/
    #  (-) 7*------*8
    
    keystr = str(ctrX) + " " + str(ctrY) + " " + str(ctrZ)
    
    if keystr in elev:
        ctrY += elev[keystr]
    else:
        elev[keystr] = 0.0

    size = 0.4
    rise = 0.005

    elev[keystr] += (rise * 3)
    
    p1X = ctrX - size;   p1Y = ctrY + rise;   p1Z = ctrZ - size 
    p2X = ctrX + size;   p2Y = ctrY + rise;   p2Z = ctrZ - size
    p3X = ctrX - size;   p3Y = ctrY + rise;   p3Z = ctrZ + size
    p4X = ctrX + size;   p4Y = ctrY + rise;   p4Z = ctrZ + size
    p5X = ctrX - size;   p5Y = ctrY - rise;   p5Z = ctrZ - size
    p6X = ctrX + size;   p6Y = ctrY - rise;   p6Z = ctrZ - size
    p7X = ctrX - size;   p7Y = ctrY - rise;   p7Z = ctrZ + size
    p8X = ctrX + size;   p8Y = ctrY - rise;   p8Z = ctrZ + size
    
    rank_str = ""
    rank_str += str(p1X) + " " + str(p1Y) + " " + str(p1Z) + " "
    rank_str += str(p2X) + " " + str(p2Y) + " " + str(p2Z) + " "
    rank_str += str(p4X) + " " + str(p4Y) + " " + str(p4Z) + " "
    rank_str += str(p3X) + " " + str(p3Y) + " " + str(p3Z) + " "
    rank_str += str(p5X) + " " + str(p5Y) + " " + str(p5Z) + " "
    rank_str += str(p6X) + " " + str(p6Y) + " " + str(p6Z) + " "
    rank_str += str(p8X) + " " + str(p8Y) + " " + str(p8Z) + " "
    rank_str += str(p7X) + " " + str(p7Y) + " " + str(p7Z)
    
    return rank_str
    #end:def xyzToHexStringStackedWafers(...)
   


def xyzToHexStringRandomScatter(ctrX, ctrY, ctrZ, state):
    #         1*______*2 (+)
    #         /|     /|
    #        / |    / |
    #      3*------*4 |
    #       | 5*___|__*6
    #       | /    | /
    #       |/     |/
    #  (-) 7*------*8

    ctrX = ctrX + (0.5 * random.random())
    ctrY = ctrY + (0.5 * random.random())
    ctrZ = ctrZ + (0.5 * random.random())

    rise = size = 0.3

    p1X = ctrX - size;   p1Y = ctrY + rise;   p1Z = ctrZ - size 
    p2X = ctrX + size;   p2Y = ctrY + rise;   p2Z = ctrZ - size
    p3X = ctrX - size;   p3Y = ctrY + rise;   p3Z = ctrZ + size
    p4X = ctrX + size;   p4Y = ctrY + rise;   p4Z = ctrZ + size
    p5X = ctrX - size;   p5Y = ctrY - rise;   p5Z = ctrZ - size
    p6X = ctrX + size;   p6Y = ctrY - rise;   p6Z = ctrZ - size
    p7X = ctrX - size;   p7Y = ctrY - rise;   p7Z = ctrZ + size
    p8X = ctrX + size;   p8Y = ctrY - rise;   p8Z = ctrZ + size

    rank_str = ""
    rank_str += str(p1X) + " " + str(p1Y) + " " + str(p1Z) + " "
    rank_str += str(p2X) + " " + str(p2Y) + " " + str(p2Z) + " "
    rank_str += str(p4X) + " " + str(p4Y) + " " + str(p4Z) + " "
    rank_str += str(p3X) + " " + str(p3Y) + " " + str(p3Z) + " "
    rank_str += str(p5X) + " " + str(p5Y) + " " + str(p5Z) + " "
    rank_str += str(p6X) + " " + str(p6Y) + " " + str(p6Z) + " "
    rank_str += str(p8X) + " " + str(p8Y) + " " + str(p8Z) + " "
    rank_str += str(p7X) + " " + str(p7Y) + " " + str(p7Z)
    
    return rank_str
    #end:def xyzToHexStringRandScatter(...)

def printf(format, *args):
    sys.stdout.write(format % args)

###############################################################################
###############################################################################

if __name__ == "__main__":
    sosVTKProjector()
    #end:FILE


