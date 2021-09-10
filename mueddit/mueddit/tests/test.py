#!/usr/bin/python3

import unittest

from mueddit import make_dawg, Iterator

class TestIterator(unittest.TestCase):
    def test_initial_final(self):
        dawg = make_dawg(( '', 'a' ))
        found = set(Iterator('b', 1, dawg))
        self.assertEqual({ '', 'a' }, found)

    def test_foo(self):
        dawg = make_dawg(( 'foo', 'bar' ))
        found = set(Iterator('baz', 1, dawg))
        self.assertEqual({ 'bar' }, found)

        found = set(Iterator('baz', 2, dawg))
        self.assertEqual({ 'bar' }, found)

    def test_this(self):
        dictionary = ( 'this', 'that', 'other' )
        dawg = make_dawg(dictionary)
        found = set(Iterator('the', 1, dawg))
        self.assertEqual(set(), found)

        found = set(Iterator('the', 2, dawg))
        self.assertEqual(set(dictionary), found)

    def test_long_head(self):
        found = set(Iterator('abtrtz', 1, make_dawg(( 'abtrbtz', ))))
        self.assertEqual({ 'abtrbtz' }, found)

    def test_tolerance(self):
        dictionary = ( 'meter', 'otter', 'potter' )
        dawg = make_dawg(dictionary)
        found = set(Iterator('mutter', 1, dawg))
        self.assertEqual(set(), found)

        found = set(Iterator('mutter', 2, dawg))
        self.assertEqual(set(dictionary), found)

    def test_binary(self):
        dictionary = ( 'ababa', 'babab' )
        dawg = make_dawg(dictionary)
        found = set(Iterator('abba', 3, dawg))
        self.assertEqual(set(dictionary), found)

if __name__ == '__main__':
    unittest.main()
