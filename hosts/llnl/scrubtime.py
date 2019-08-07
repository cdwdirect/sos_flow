#!/usr/bin/python
import csv
import io
with io.open('times.csv', 'r', newline='') as csvfile:
    data = csv.reader(csvfile)
    for time_iter, start_time, start_sec, stop_time, stop_sec in data:
        print time_iter + ' : ' + str(int(stop_sec) - int(start_sec))

