#!/usr/bin/python3

from mueddit import make_dawg, Iterator
from ingest import make_test_dict
import argparse
import csv
from Levenshtein import distance
import os.path
import re

def test_independent(seen, n, dictionary, dawg, writer):
    external = set()
    for correct in dictionary:
        if distance(seen, correct) <= n:
            external.add(correct)

    row = [ seen ]
    internal = set()
    for found in Iterator(seen, n, dawg):
        row.append(found)
        internal.add(found)

    writer.writerow(row)

    if external != internal:
        raise Exception("results for %s differ" % seen)

def test_repeat(seen, n, dawg, reader):
    row = next(reader)
    if not len(row):
        raise Exception("empty row")

    if row[0] != seen:
        raise Exception("result row start mismatch")

    i = 1
    for found in Iterator(seen, n, dawg):
        if row[i] != found:
            raise Exception("result row mismatch")

        i += 1

    if i != len(row):
        raise Exception("result row end mismatch")

def main():
    parser = argparse.ArgumentParser(description='MUlti-word EDit DIstance test')
    parser.add_argument('--tolerance', '-t', type=int, default=1, help='max allowed number of edits')
    parser.add_argument('input', nargs=1, help='input file path')
    parser.add_argument('--result', '-r', type=str, default='result.tsv', help='input file path')
    parser.add_argument('--single-dict', '-s', action='store_true', help='include tested word in the dictionary')
    args = parser.parse_args()

    n = args.tolerance
    if n <= 0:
        raise Exception("max allowed number of edits must be positive")

    input_path = os.path.abspath(args.input[0])
    result_path = args.result
    single_mode = args.single_dict

    dictionary = make_test_dict(input_path)

    dd = None
    dawg = None
    if single_mode:
        dd = dictionary
        dawg = make_dawg(dd)

    if not os.path.isfile(result_path):
        with open(result_path, 'w', newline='') as f:
            writer = csv.writer(f, delimiter='\t')
            writer.writerow([ input_path, str(n), int(single_mode)])
            for tword in sorted(dictionary):
                print("%s..." % tword)

                if not single_mode:
                    dd = dictionary - { tword }
                    dawg = make_dawg(dd)

                test_independent(tword, n, dd, dawg, writer)
    else:
        with open(result_path, newline='') as f:
            reader = csv.reader(f, delimiter='\t')
            first_row = next(reader)
            if len(first_row) != 3:
                raise Exception("three-column header expected")

            if (first_row[0] != input_path) or (int(first_row[1]) != n) or (int(first_row[2]) != int(single_mode)):
                raise Exception("inputs changed")

            for tword in sorted(dictionary):
                print("%s..." % tword)

                if not single_mode:
                    dawg = make_dawg(dictionary - { tword })

                test_repeat(tword, n, dawg, reader)

if __name__ == '__main__':
    main()
