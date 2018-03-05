#!/usr/bin/env python
import os
import sys
import sqlite3
import numpy as np
import pylab as pl
import time
import random
import matplotlib.pyplot as plt

conn = None

##############################################################################
# A Density-Based Algorithm for Discovering Clusters in Large Spatial Databases with Noise
# Martin Ester, Hans-Peter Kriegel, Jorg Sander, Xiaowei Xu
# dbscan: density based spatial clustering of applications with noise

import math

UNCLASSIFIED = False
NOISE = None

def _dist(p,q):
	return math.sqrt(np.power(p-q,2).sum())

def _eps_neighborhood(p,q,eps):
	return _dist(p,q) < eps

def _region_query(m, point_id, eps):
    n_points = m.shape[1]
    seeds = []
    for i in range(0, n_points):
        if _eps_neighborhood(m[:,point_id], m[:,i], eps):
            seeds.append(i)
    return seeds

def _expand_cluster(m, classifications, point_id, cluster_id, eps, min_points):
    seeds = _region_query(m, point_id, eps)
    if len(seeds) < min_points:
        classifications[point_id] = NOISE
        return False
    else:
        classifications[point_id] = cluster_id
        for seed_id in seeds:
            classifications[seed_id] = cluster_id
            
        while len(seeds) > 0:
            current_point = seeds[0]
            results = _region_query(m, current_point, eps)
            if len(results) >= min_points:
                for i in range(0, len(results)):
                    result_point = results[i]
                    if classifications[result_point] == UNCLASSIFIED or \
                       classifications[result_point] == NOISE:
                        if classifications[result_point] == UNCLASSIFIED:
                            seeds.append(result_point)
                        classifications[result_point] = cluster_id
            seeds = seeds[1:]
        return True
        
def dbscan(m, eps, min_points):
    """Implementation of Density Based Spatial Clustering of Applications with Noise
    See https://en.wikipedia.org/wiki/DBSCAN
    
    scikit-learn probably has a better implementation
    
    Uses Euclidean Distance as the measure
    
    Inputs:
    m - A matrix whose columns are feature vectors
    eps - Maximum distance two points can be to be regionally related
    min_points - The minimum number of points to make a cluster
    
    Outputs:
    An array with either a cluster id number or dbscan.NOISE (None) for each
    column vector in m.
    """
    cluster_id = 1
    n_points = m.shape[1]
    classifications = [UNCLASSIFIED] * n_points
    for point_id in range(0, n_points):
        point = m[:,point_id]
        if classifications[point_id] == UNCLASSIFIED:
            if _expand_cluster(m, classifications, point_id, cluster_id, eps, min_points):
                cluster_id = cluster_id + 1
    return classifications

def test_dbscan():
    m = np.matrix('1 1.2 0.8 3.7 3.9 3.6 10; 1.1 0.8 1 4 3.9 4.1 10')
    eps = 0.5
    min_points = 2
    assert dbscan(m, eps, min_points) == [1, 1, 1, 2, 2, 2, None]

def dbscan_vector(vector, eps, min_points):
    m = np.matrix(vector)
    """
    # get the distances between all points
    distances = []
    for i in range(0,len(vector)):
        for j in range(i+1,len(vector)):
            distances.append(np.abs(vector[i] - vector[j]))
    # make an autobinning histogram of the values
    """
    hist,binedges = np.histogram(vector,bins='rice')
    #print hist
    right = 0.0
    left = 0.0
    foundright = False
    for i in range(len(hist)-1,0,-1):
        if hist[i] == 0:
            right = binedges[i+1]
            foundright = True
        elif hist[i] > 0 and foundright:
            left = binedges[i+1]
            eps = right - left
            break
    #print eps
    clusters = dbscan(m, eps, min_points)
    # found out which cluster has largest values
    subclust = {}
    maxval = 0.0
    maxc = 0
    for c,v in zip(clusters,vector):
        if c != None:
            if c not in subclust:
                subclust[c] = []
            subclust[c].append(v)
            if v > maxval:
                maxval = v
                maxc = c
                #print maxval,maxc
    return clusters,len(subclust),maxc,np.min(subclust[maxc])
##############################################################################

# Make a connection to the SQLite3 database
def open_connection(filename):
    global conn
    # check for file to exist
    print ("Checking for file: ", sqlite_file)
    while not os.path.exists(sqlite_file):
        print ("Waiting on file: ", sqlite_file)
        time.sleep(1)

    print("Connecting to: ", sqlite_file)
    # Connecting to the database file
    conn = sqlite3.connect(sqlite_file)
    #fd = os.open(sqlite_file, os.O_RDONLY)
    #conn = sqlite3.connect('/dev/fd/%d' % fd)
    url = 'file:' + sqlite_file + '?mode=ro'
    #url = 'file:' + sqlite_file
    #conn = sqlite3.connect(url, uri=True)
    conn.isolation_level=None
    c = conn.cursor()
    #c.execute('PRAGMA journal_mode=WAL;')
    #c.execute('PRAGMA synchronous   = ON;')
    #c.execute('PRAGMA cache_size    = 31250;')
    #c.execute('PRAGMA cache_spill   = FALSE;')
    #c.execute('PRAGMA temp_store    = MEMORY;')
    return c

# wrapper around queries, for error handling
def try_execute(c, statement, parameters=None):
    success = False
    #print(statement)
    while not success:
        try:
            if parameters:
                c.execute(statement,parameters);
            else:
                c.execute(statement);
            success = True
            break;
        except sqlite3.Error as e:
            #print("database error...", e.args[0])
            success = False

# find all of the ranks participating in the simulation, sending data over SOS
def get_ranks(c,application):
    sql_statement = ("select distinct guid,comm_rank from tblPubs where prog_name like '%" + application + "%' order by comm_rank;")
    #print("Executing query")
    try_execute(c,sql_statement);
    all_rows = c.fetchall()
    pub_guids = np.array([x[0] for x in all_rows])
    ranks = np.array([x[1] for x in all_rows])
    ranklen = len(ranks)
    print ("pub_guids: ", pub_guids)
    print ("ranks: ", ranks)
    return pub_guids, ranks

# Get the GUIDs for the MPI Collective events, for this publisher GUID (i.e. for this rank).
def get_mpi_collective_guid(pg):
    sql_statement = ("select guid from tblData where pub_guid = " + str(pg) + " and name like 'MPI collective%';")
    try_execute(c,sql_statement);
    all_rows = c.fetchall()
    guid = np.array([x[0] for x in all_rows])
    # print "MPI collective exchange guid: ", guid
    return guid[0]

# Get the last n start and stop timestamps for all MPI collective events for this rank. 
# First, sort them by "most recent first", then take those n rows and reverse the order 
# so we have them in chronological order.
def get_mpi_exchanges(mpi_guid,limit):
    sql_statement = ("select start, end from (select time_pack-val as start, time_pack as end from tblVals where guid = " + str(mpi_guid) + " order by end DESC limit " + limit + ") order by end ASC;")
    try_execute(c,sql_statement);
    all_rows = c.fetchall()
    starts = np.array([x[0] for x in all_rows])
    ends = np.array([x[1] for x in all_rows])
    return starts,ends

# Just take the difference between the two arrays, start[n] - end[n-1] for all n.
# The resulting array should have n-1 elements.
def find_gap_between_timestamps(starts,ends):
    durations = np.zeros(len(ends)-1)
    for i in range(1,len(ends)):
        durations[i-1] = starts[i]-ends[i-1]
    #print durations
    return durations

# For validation.
def find_nth_percentile(durations,n):
    print n, "th percentile:", np.percentile(durations,n)

# For validation.
def reject_outliers(data, m=2):
    means = data[abs(data - np.mean(data)) < m * np.std(data)]
    medians = data[abs(data - np.median(data)) < m * np.std(data)]
    #print np.min(means), np.mean(means), np.median(means), np.max(means)
    #print np.min(medians), np.mean(medians), np.median(medians), np.max(medians)
    return means,medians

def get_first_arrival(data):
    return np.min(data)

def get_last_arrival(data):
    return np.max(data)

# name of the sqlite database file
sqlite_file = sys.argv[1]
application = sys.argv[2]

# open the connection
c = open_connection(sqlite_file)

rows=100

# get the publisher guids, ranks
pub_guids,ranks = get_ranks(c, application)
# declare some arrays for averaging across ranks
mean_arrivals = np.zeros(len(pub_guids))
last_windows = np.zeros(len(pub_guids))
next_windows = np.zeros(len(pub_guids))
all_starts = np.zeros(len(pub_guids)*rows)
all_ends = np.zeros(len(pub_guids)*rows)
index = 0;
# do this in parallel when in C!
for pg in pub_guids:
    # get guid for MPI collectives
    mpi_guid = get_mpi_collective_guid(pg)
    # Get the start, stop times for last n MPI Collectives
    starts,ends = get_mpi_exchanges(mpi_guid,str(rows))
    j = 0
    for s,e in zip(starts,ends):
        all_starts[(rows*index)+j] = s
        all_ends[(rows*index)+j] = e
        j = j + 1
    # How long are the compute windows?
    durations = find_gap_between_timestamps(starts,ends)
    print "clustering..."
    clusters,numclust,maxc,maxv = dbscan_vector(durations, 0.5, 2)
    print numclust
    #print durations
    print clusters,numclust,maxc,maxv
    # What is the periodicity of the windows?
    # filter out any durations not in the largest cluster values
    if numclust > 1:
        newstarts = []
        newends = []
        newdurations = []
        for i in range(0,len(clusters)):
            if clusters[i] == maxc:
                newstarts.append(starts[i])
                newends.append(ends[i])
                newdurations.append(durations[i])
        starts = newstarts
        ends = newends
        durations = newdurations
        print durations
        # STart over, just with the largest windows.
        # How long are the compute windows?
        # durations = find_gap_between_timestamps(starts,ends)
    periods = find_gap_between_timestamps(ends,ends)
    last_windows[index] = np.max(ends)
    next_windows[index] = np.mean(periods)
    #find_nth_percentile(durations,70)
    means,medians = reject_outliers(np.array(durations))
    #means = durations
    mean_arrivals[index] = np.mean(means)
    index = index + 1

# filter out the zeros from the numpy arrays
all_starts = np.ma.masked_equal(all_starts,0).compressed()
all_ends = np.ma.masked_equal(all_ends,0).compressed()
time_start = 0
if False:
    time_start = all_starts[0]

print "periodicity: ", np.mean(periods)
print "compute duration, first arrival: ", get_first_arrival(mean_arrivals), ", last arrival: ", get_last_arrival(mean_arrivals)
print "last window: "
print np.mean(last_windows) - time_start, "to", np.mean(last_windows) + get_first_arrival(mean_arrivals) - time_start
print "next 3 windows: "
print np.mean(last_windows) + np.mean(next_windows) - time_start, "to", np.mean(last_windows) + np.mean(next_windows) + get_first_arrival(mean_arrivals) - time_start
print np.mean(last_windows) + 2 * np.mean(next_windows) - time_start, "to", np.mean(last_windows) + (2 * np.mean(next_windows)) + get_first_arrival(mean_arrivals) - time_start
print np.mean(last_windows) + 3 * np.mean(next_windows) - time_start, "to", np.mean(last_windows) + (3 * np.mean(next_windows)) + get_first_arrival(mean_arrivals) - time_start

print("Closing connection to database.")
conn.close()
print("Done.")

#print len(all_starts)
#print len(all_ends)
#print all_starts
#print all_ends

all_timestamps = []
all_counts = []
all_starts = np.sort(all_starts)
all_ends = np.sort(all_ends)
s = 0
e = 0
i = 0
t = 0
while s < len(all_starts) and e < len(all_ends):
    if all_starts[s] < all_ends[e]:
        if t == 0:
            all_timestamps.append(all_starts[s] - time_start)
            all_counts.append(0)
            i = i + 1
        all_timestamps.append(all_starts[s] - time_start)
        t = t + 1
        all_counts.append(t)
        s = s + 1
    else:
        if t == len(pub_guids):
            all_timestamps.append(all_ends[e] - time_start)
            all_counts.append(t)
            i = i + 1
        all_timestamps.append(all_ends[e] - time_start)
        t = t - 1
        all_counts.append(t)
        e = e + 1
    i = i + 1
for j in range(0,len(pub_guids)):
    if t == 0:
       break
    all_timestamps.append(all_ends[e] - time_start)
    t = t - 1
    all_counts.append(t)
    e = e + 1
    i = i + 1

#print all_timestamps
#print all_counts
plt.plot(all_timestamps, all_counts)
plt.ylabel('Num Ranks at Collective')
plt.show()
