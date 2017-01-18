#!/usr/bin/python 

import simplejson as json
from pprint import pprint
import argparse
import subprocess
import os
import shlex
import time
import sys, traceback

filename = "not-specified.json"
commands = []
sos_root = ""

"""
This script expects a JSON file with a format something like this:
{
    "nodes": [{
        "name": "a",
        "mpi_ranks": "1",
        "children": ["b"],
        "parents": [],
        "iterations": 10
    }, {
        "name": "b",
        "mpi_ranks": "1",
        "children": [],
        "parents": ["a"]
    }],
    "sos_root": "/home3/khuck/src/sos_flow",
    "sos_num_daemons": "1",
    "sos_cmd_port": "22500",
    "sos_cmd_buffer_len": "8388608",
    "sos_num_dbs": "1",
    "sos_db_port": "22503",
    "sos_db_buffer_len": "8388608",
    "sos_working_dir": "/tmp/sos_flow_working"
}
"""

# This method will parse the arguments, of which there is one - the JSON file.
def parse_arguments():
    parser = argparse.ArgumentParser(description='Generate and execute a generic workflow example.')
    parser.add_argument('filename', metavar='filename', type=str, help='a JSON input config file')
    args = parser.parse_args()
    return args.filename

# this method will generate the ADIOS XML file for the FLEXPATH writer. 
# Unfortunately, we need the XML file for ADIOS to work correctly.
# The code will replace the string REPLACEME with the name of the communication
# relationship, something like a_to_b or foo_to_bar - the same thing that the
# flexpath_writer.c and flexpath_reader.c source code expects.
def generate_xml(data):
    header = """<?xml version="1.0"?>\n    <adios-config host-language="C">\n"""
    footer = """<buffer size-MB="20" allocate-time="now"/>\n</adios-config>\n"""
    group_data = """    <adios-group name="REPLACEME" coordination-communicator="comm" stats="On">
        <var name="NX" path="/scalar/dim" type="integer"/>
        <var name="NY" path="/scalar/dim" type="integer"/>
        <var name="NZ" path="/scalar/dim" type="integer"/>
        <var name="size" type="integer"/>
        <var name="rank" type="integer"/>
        <var name="offset" type="integer"/>
        <var name="size_y" type="integer"/>
        <var name="shutdown" type="integer"/>
        <global-bounds dimensions="/scalar/dim/NZ,size_y,/scalar/dim/NX" offsets="0,offset,0">
            <var name="var_2d_array" gwrite="t" type="double" dimensions="/scalar/dim/NZ,/scalar/dim/NY,/scalar/dim/NX"/>
        </global-bounds>
    </adios-group>\n"""
    group_method = """    <method group="REPLACEME" method="FLEXPATH">QUEUE_SIZE=10</method>\n"""
    f = open('arrays.xml', 'w')
    f.write(header)
    # iterate over the nodes, and for each one, iterate over its children
    # to generate the adios-group node in the XML
    for node in data["nodes"]:
        for child in node["children"]:
            replacement = node["name"] + "_to_" + child
            f.write(group_data.replace("REPLACEME", replacement))
    # iterate over the nodes, and for each one, iterate over its children
    # to generate the adios method node in the XML
    for node in data["nodes"]:
        for child in node["children"]:
            replacement = node["name"] + "_to_" + child
            f.write(group_method.replace("REPLACEME", replacement))
    f.write(footer)
    f.close()

# This method will parse the PBS node file in order to get a list of hostnames
# and a set of unique hostnames in order to generate an openmpi hostfile for
# each node in the workflow.  If we are using a different queue (like slurm) or
# a different MPI (like mpich) then we will have to add additional support.
def parse_nodefile():
    # SLURM_JOB_NODELIST=nid000[16-17]
    filename = os.environ['SLURM_JOB_NODELIST']
    arguments = "scontrol show hostname " + filename
    args = shlex.split(arguments)
    f = subprocess.check_output(args)
    hostnames = []
    unique_hostnames = set()
    for line in shlex.split(f):
        print line
        hostnames.append(line.strip())
        unique_hostnames.add(line.strip())
    return hostnames, unique_hostnames

# This method will generate the srun commands for each node in the workflow
# graph. It should generate something like this:
# "srun -np 1 --hostfile hostfile_a /sos_flow/bin/generic_node --name a --iterations 10 --writeto b"
def generate_commands(data, hostnames):
    commands = []
    logfiles = []
    profiledirs = []
    global sos_root
    sos_root = data["sos_root"]
    index = 0
    for node in data["nodes"]:
        hostfile_arg = " -m arbitrary -w " + hostnames[index]
        index = index + 1
        command = "srun -n " + node["mpi_ranks"] + hostfile_arg + " -N 1 --gres=craynetwork:1 --mem=25600 -l " + data["sos_examples_bin"] + "/generic_node --name " + node["name"]
        if "iterations" in node:
            command = command + " --iterations " + str(node["iterations"])
        #command = command + " --iterations " + str(data["iterations"])
        for child in node["children"]:
            command = command + " --writeto " + child
        for parent in node["parents"]:
            command = command + " --readfrom " + parent
        commands.append(command)
        logfile = node["name"] + ".log"
        logfiles.append(logfile)
        profiledir = "profiles_" + node["name"]
        profiledirs.append(profiledir)
    return commands,logfiles,profiledirs
    
# this method will use the environment variables and JSON config settings to
# launch the SOS daemons (one per node) and the SOS database (at least one), as
# well as each of the nodes in the workflow. All system calls are done in the
# background, with the exception of the last one - this script will wait for
# that one to finish before continuing.
def execute_commands(commands, logfiles, profiledirs, data, unique_hostnames):
    sos_root = data["sos_root"]
    sos_bin = data["sos_bin"]
    sos_examples_bin = data["sos_examples_bin"]
    sos_working_dir = data["sos_working_dir"]
    sos_num_daemons = data["sos_num_daemons"]
    sos_cmd_port = data["sos_cmd_port"]
    sos_num_dbs = data["sos_num_dbs"]
    sos_db_port = data["sos_db_port"]
    os.environ['SOS_ROOT'] = sos_root
    os.environ['SOS_WORK'] = sos_working_dir
    os.environ['SOS_CMD_PORT'] = sos_cmd_port
    os.environ['SOS_DB_PORT'] = sos_db_port
    daemon = sos_bin + "/sosd"
    sos_num_daemons = len(unique_hostnames)
    """
    hostfile = "hostfile_sos"
    f = open (hostfile, 'w')
    for i in unique_hostnames:
        f.write(i + "\n")
    f.close()
    """
    # launch the SOS daemon(s) and SOS database(s)
    #arguments = "srun --wait 60 -n ${SLURM_NNODES} -N ${SLURM_NNODES} -c 4 --hint=multithread --gres=craynetwork:1 --mem=25600 -l ${SOS_ROOT}/build-cori/bin/sosd -l 1 -a 1 -w ${SOS_WORK}"
    arguments = "srun --wait 60 -n " + str(sos_num_daemons) + " -N " + str(sos_num_daemons) + " -c 4 --hint=multithread --gres=craynetwork:1 --mem=25600 -l " + daemon + " -l " + str(sos_num_daemons-1) + " -a 1 -w " + sos_working_dir 
    print arguments
    args = shlex.split(arguments)
    subprocess.Popen(args)
    time.sleep(10)
    index = 0
    openfiles = []
    # launch all of the nodes in the workflow
    for command,logfile,profiledir in zip(commands,logfiles,profiledirs):
        os.environ['PROFILEDIR'] = profiledir
        makedir = "mkdir -p " + profiledir
        args = shlex.split(makedir)
        subprocess.call(args)
        index = index + 1
        print command
        args = shlex.split(command)
        #lf = open(logfile,'w')
        #openfiles.append(lf)
        if index == len(commands):
            #subprocess.check_call(args, stdout=lf, stderr=subprocess.STDOUT)
            subprocess.check_call(args, stderr=subprocess.STDOUT)
        else:
            #subprocess.Popen(args, stdout=lf, stderr=subprocess.STDOUT)
            subprocess.Popen(args, stderr=subprocess.STDOUT)
        time.sleep(1)
    # close the log files
    for lf in openfiles:
        lf.close()

    # shut down - wait for a bit so we can shutdown the database server cleanly.
    print "Waiting for all processes to finish..."
    time.sleep(5)
    print "Stopping the database..."
    arguments = "srun --wait 60 --gres=craynetwork:1 --mem=25600 -n " + str(sos_num_daemons-1) + " " + data["sos_bin"] + "/sosd_stop"
    print arguments
    args = shlex.split(arguments)
    subprocess.call(args)

# main, baby - main!
def main():
    global sos_root
    filename = parse_arguments()
    with open(filename) as data_file:    
        data = json.load(data_file)
    #pprint(data)
    generate_xml(data)
    hostnames, unique_hostnames = parse_nodefile()
    commands,logfiles,profiledirs = generate_commands(data, hostnames)
    try:
        execute_commands(commands, logfiles, profiledirs, data, unique_hostnames)
    except:
        print "failed!"
        traceback.print_exc(file=sys.stderr)
        sos_num_daemons = len(unique_hostnames)
        arguments = "srun --wait 60 --gres=craynetwork:1 --mem=25600 -n " + str(sos_num_daemons-1) + " " + data["sos_bin"] + "/sosd_stop"
        print arguments
        args = shlex.split(arguments)
        subprocess.call(args)

if __name__ == "__main__":
    main()

