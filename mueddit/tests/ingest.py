#!/usr/bin/python3

from mueddit import make_dawg
import os
import re
import sys

def make_test_dict(input_path):
    dictionary = set()
    with open(input_path) as f:
        # this may combine adjacent words, but that can be construed
        # as a feature, making the dictionary more interesting...
        unword_rx = re.compile("\\W")
        for ln in f:
            for raw in ln.split():
                word = unword_rx.sub("", raw.strip())
                if word:
                    dictionary.add(word)

    return dictionary

def count_children(st, totals):
    keys = st.keys()
    idx = len(keys)
    while len(totals) <= idx:
        totals.append(0)

    totals[idx] += 1

    for k in keys:
        count_children(st.get_child(k), totals)

def main():
    if len(sys.argv) != 2:
        raise Exception("usage: %s input-file" % sys.argv[0])

    input_path = os.path.abspath(sys.argv[1])
    dictionary = make_test_dict(input_path)
    dawg = make_dawg(dictionary)
    totals = []
    count_children(dawg.root, totals)
    i = 0
    l = len(totals)
    while i < l:
        if totals[i] > 0:
            print("%d: %d" % (i, totals[i]))

        i += 1

if __name__ == '__main__':
    main()
