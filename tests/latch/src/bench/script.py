import csv
import json
import math
import os
import re

import scipy

esp = {}
emu = {}

# init dictionaries

dir = '../../experiments/pc2/'
for file in filter(lambda f: f.endswith('.log'), os.listdir(dir)):
    f = open(dir + file, "r")
    suite = json.loads(f.read())
    f.close()

    if 'passes' in suite.keys() and suite['passes'] != []:
        passes = suite['passes'][0]
        match = re.search(r' (?P<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast', passes)
        asserts = suite['passed actions']

        esp[match.group('name')] = {
            'size': asserts,
            'durations': []
        }

        emu[match.group('name')] = {
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
            passes = suite['passes'][0]
            match = re.search(r' (?P<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast', passes)
            duration = suite['duration (ms)']

            esp[match.group('name')]["durations"].append(duration)
        else:
            print(suite)

# parse emulator results

dir = '../../experiments/pc2/'
for file in filter(lambda f: f.endswith('.log'), os.listdir(dir)):
    f = open(dir + file, "r")
    suite = json.loads(f.read())
    f.close()

    if 'passes' in suite.keys() and len(suite['passes']) > 1:
        passes = suite['passes'][0]
        match = re.search(r' (?P<name>[^\n _]*_??[^\n _]*)_?[0-9]*.wast', passes)
        duration = suite['duration (ms)']

        emu[match.group('name')]["durations"].append(duration)
    else:
        print(suite)

# create csv

rows = []
for point in esp:
    mean = scipy.stats.gmean(esp[point]['durations'])
    if not math.isnan(mean):
        rows.append([point, esp[point]['size'], round(mean),
                     round(scipy.stats.gstd(esp[point]['durations']), 2),
                     round(scipy.stats.gmean(emu[point]['durations'])),
                     0
                     ])

rows.sort(key=lambda row: row[1])

with open('../../experiments/runtime.csv', 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["name", "asserts", "hardware", "hardware-deviation", "emulator", "emulator-deviation"])
    writer.writerows(rows)
