#!/usr/bin/python
#####!/usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/python/bin/python

# from   mpl_toolkits.mplot3d import   Axes3D
# from   matplotlib.colors    import   Normalize
# import matplotlib.cm     as  cm
# import matplotlib.pyplot as  plt

import   sys
import   subprocess
import   time
import   os
import   random
import   vtk_writer
import   numpy  as  np
from     ssos   import SSOS

def queryAndPlot():
    SOS = SSOS()

    print "Initializing SOS..."
    SOS.init()
    print "DONE init SOS..."

    #sql_string = """
    #SELECT MAX(frame) FROM viewCombined
    #;
    #"""
    #results, col_names = SOS.query(sql_string,
    #        "localhost",
    #        os.environ.get("SOS_CMD_PORT"))
    # 
    #if len(results[0]) > 0:
    #    max_cycle = int(results[0][0])
    #else:
    max_cycle = 1
    print "Max cycle: " + str(max_cycle)

    #####
    #
    #  Get the list of field names for non-string values.
    #
    sql_string = """
    SELECT
    DISTINCT value_name
    FROM viewCombined
    WHERE value_type != 3
    AND frame = """ + str(max_cycle) + """
    ;
    """
    results, col_names = SOS.query(sql_string,
            "localhost", 
            os.environ.get("SOS_CMD_PORT"))
    numeric_fields = dict()
    numeric_fields['name'] = [el[0] for el in results]
    name_count = len(numeric_fields['name'])
    print str(name_count) + " unique names."
    #
    #####


    filenames = [] 
    print "******* CYCLE " + str(max_cycle) +" *********"
    #####
    #
    #  Compose a query with the unique numeric fields as columns:
    #
    sql_string  = """ """
    sql_string += """ SELECT """
    sql_string += """ comm_rank """
    # sql_string += """,frame """
    for field_name in numeric_fields['name']:
        sql_string += """,GROUP_CONCAT( CASE WHEN """
        sql_string += ' value_name LIKE "' + field_name + '" '
        sql_string += ' THEN value END) AS "' + field_name + '" '
    #sql_string += """, GROUP_CONCAT( CASE WHEN """
    #sql_string += ' value_name LIKE "%CRAY_PMI_X%" '
    #sql_string += ' THEN value END) AS "X" '
    #sql_string += """, GROUP_CONCAT( CASE WHEN """
    #sql_string += ' value_name LIKE "%CRAY_PMI_Y%" '
    #sql_string += ' THEN value END) AS "Y" '
    #sql_string += """, GROUP_CONCAT( CASE WHEN """
    #sql_string += ' value_name LIKE "%CRAY_PMI_Z%" '
    #sql_string += ' THEN value END) AS "Z" '
    sql_string += """ FROM viewCombined """
    sql_string += " WHERE frame = " + str(max_cycle) + " " 
    sql_string += """ GROUP BY """
    sql_string += """ comm_rank """
    # sql_string += """,frame """
    sql_string += """;"""
    print "Composite SQL statement: "
    print sql_string
    print ""
    #print "Running composite query..."
    results, col_names = SOS.query(sql_string,
            "localhost",
            os.environ.get("SOS_CMD_PORT"))
    print ""
    #
    #  Print out the results:
    #
    print "=========="
    for col in col_names:
        print str(col) + " "
    print "=========="
    for row in results:
        for col_index in range(len(row)):
            print str(col_names[col_index]) + ": " + str(row[col_index])
        print "----------"
    print "=========="
    #
    #####

    #####
    #
    #  Build an attribute dictionary of the values.
    #
    attr = dict()
    attr['comm_rank']  =  [el[0] for el in results]

    position = 1
    for field_name in numeric_fields['name']:
        attr[field_name] = [el[position] for el in results]
        print str(field_name) + " in position " + str(position) + " = " + str(attr[field_name])
        position += 1
    #res_coords = [el[position] for el in results]
    #print "lulesh.coords in position " + str(position) + " = " + str(res_coords)

    for field_name in numeric_fields['name']:
        rank = 0
        for this_ranks_value in attr[field_name]:
            print "comm_rank(" + str(rank) + ")." + field_name + " = " + this_ranks_value
            rank += 1

#SELECT  X, Y, COUNT(comm_rank) FROM (SELECT comm_rank, GROUP_CONCAT( CASE WHEN  value_name LIKE "%CRAY_PMI_X"  THEN value END) AS
#"X", GROUP_CONCAT( CASE WHEN  value_name LIKE "%CRAY_PMI_Y"  THEN value END) AS "Y" FROM viewCombined);


    rank_max = len(attr['comm_rank'])

    res_coords = list()
    elev = {}

    size = 0.25;
    rise = 0.05;

    for rank in range(rank_max):
        ctrX = float(attr['TAU::0::Metadata::CRAY_PMI_X'][rank])
        ctrY = float(attr['TAU::0::Metadata::CRAY_PMI_Y'][rank])
        ctrZ = float(attr['TAU::0::Metadata::CRAY_PMI_Z'][rank])

        #ctrX = ctrX + (0.5 * random.random())
        #ctrY = ctrY + (0.5 * random.random())
        #ctrZ = ctrZ + (0.5 * random.random())

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
        rank_str += str(p7X) + " " + str(p7Y) + " " + str(p7Z) + " "
        res_coords.append(rank_str)

    coords = list()
    coords = [el.split() for el in res_coords]

    dset = vtk_writer.vtk_hex_data_set()
    dset.clear()
    dset.set_cycle(max_cycle)
    for rank in range(0, rank_max - 1):
        fields = {}
        for field_name in numeric_fields['name']:
            fields[field_name] = attr[field_name][rank]
        fields["rank"] = rank
        hex_coords = [None]*24
        xpt = [None]*8
        ypt = [None]*8
        zpt = [None]*8
        cpt = [None]*8
        hex_coords = res_coords[rank].split()
        print "len(hex_coords) == " + str(len(hex_coords))
        dset.add_hex(hex_coords, fields, rank)
    dset.write_vtk_file()
    filenames.append(dset.get_file_name())

    vtk_writer.write_visit_file(filenames)

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

    SOS.finalize();
    print "   ...DONE!"
    print 
    #############
  
if __name__ == "__main__":
    queryAndPlot()
    #############



