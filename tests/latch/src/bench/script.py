import csv
import json
import math
import os
import re

import numpy
import scipy

esp = {}
emu = {}

# get test suite name
def fetch_name(passes):
    if any("computational" in p.lower() for p in passes):
        return "computing"
    elif any("debugger" in p.lower() for p in passes):
        return "debugger"
    elif any("specification" in p.lower() for p in passes):
        return re.search(r' (?P<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast', passes[0]).group("name")
    else:
        print(passes)

# init dictionaries

dir = '../../experiments/pc2/'
for file in filter(lambda f: f.endswith('.log'), os.listdir(dir)):
    f = open(dir + file, "r")
    suite = json.loads(f.read())
    f.close()

    if 'passes' in suite.keys() and suite['passes'] != []:
        name = fetch_name(suite['passes'])
        asserts = suite['passed actions']

        esp[name] = {
            'size': asserts,
            'durations': []
        }

        emu[name] = {
            'size': asserts,
            'durations': []
        }

# parse hardware results

for i in range(0, 10):
    dir = f"""../../experiments/esp{i}/"""
    for file in filter(lambda f: f.endswith('.log'), os.listdir(dir)):
        f = open(dir + file, "r")
        suite = json.loads(f.read())
        f.close()

        if 'passes' in suite.keys() and len(suite['passes']) > 1:
            name = fetch_name(suite['passes'])
            duration = suite['duration (ms)']

            esp[name]["durations"].append(duration)
        else:
            print(suite)

# parse emulator results

for i in range(0, 10):
    dir = f"""../../experiments/pc{i}/"""
    for file in filter(lambda f: f.endswith('.log'), os.listdir(dir)):
        f = open(dir + file, "r")
        suite = json.loads(f.read())
        f.close()

        if 'passes' in suite.keys() and len(suite['passes']) > 1:
            name = fetch_name(suite['passes'])
            duration = suite['duration (ms)']

            emu[name]["durations"].append(duration)
        else:
            print(suite)

# create csv

rows = []
for point in esp:
    if len(esp[point]['durations']) == 0:
        continue
    hardware = esp[point]['durations']
    emulator = emu[point]['durations']
    mean = round(numpy.average(hardware))
    base = round(numpy.average(emulator))  # geometric alternative: scipy.stats.gmean
    overhead = round(mean / base, 2)
    difference = round(mean - base, 2)
    confidence = scipy.stats.ttest_ind(hardware, emulator).confidence_interval() if mean > base else scipy.stats.ttest_ind(emulator, hardware).confidence_interval()
    rows.append([point,
                 esp[point]['size'],
                 mean,
                 base,
                 overhead,
                 round(abs(difference - confidence.low), 4),  # confidence intervals
                 round(abs(difference - confidence.high), 4),
                 round(abs(overhead - abs(confidence.low / base)), 4),  # normalized confidence intervals
                 round(abs(overhead - abs(confidence.high / base)), 4)
                 ])

rows.sort(key=lambda row: row[1])

with open('../../experiments/runtime.csv', 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["name", "asserts", "hardware", "emulator", "overhead", "low", "high", "low-norm", "high-norm"])
    writer.writerows(rows)
