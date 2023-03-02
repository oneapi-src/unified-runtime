"""
 Copyright (C) 2023 Intel Corporation

 SPDX-License-Identifier: MIT

 Generates a unique id for each spec function that don't have it.
"""

from fileinput import FileInput
import util
import re

MAX_FUNCS = 1024 # We could go up to UINT32_MAX...
valid_ids = set(range(1, MAX_FUNCS))

def generate_id_str():
    return 'id: ' + str(valid_ids.pop()) + '\n'

# read all yml files looking for ids and remove them from the valid_ids set
yml_files = util.findFiles('./core/', '*.yml')
with FileInput(files=yml_files) as f:
    for line in f:
        match = re.match(r"id:\s*(\d+)", line) # match "id: number"
        if match:
            valid_ids.discard(int(match.group(1)))

# go over every function in the spec and add a unique id if it's missing
for filename in yml_files:
    with open(filename, 'r+') as f:
        infunc = False
        hasid = False

        lines = f.readlines()
        f.seek(0)
        for line in lines:
            if line.startswith('---'):
                if infunc and not hasid:
                    f.write(generate_id_str())
                infunc = False
                hasid = False

            if line.startswith('type: function'):
                infunc = True

            if line.startswith('id: '):
                hasid = True

            f.write(line)

        if infunc and not hasid:
            # print an extra newline in case it's absent
            if lines[-1][-1] != '\n':
                f.write('\n')
            f.write(generate_id_str())

