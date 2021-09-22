# cython: language_level=3

"""Deterministic acyclic finite state automaton for word sets.

AKA Directed acyclic word graph. The algorithm is from Incremental
Construction of Minimal Acyclic Finite-State Automata by Jan Daciuk,
Stoyan Mihov, Bruce W. Watson and Richard E. Watson.
"""

import sys
from mueddit.struct_hash import hash_dict

cdef class DawgState:
    """Automaton state."""
    cdef readonly bint final
    cdef object h
    cdef dict children
    cdef list sorted_keys

    def __init__(self, bint final):
        self.final = final
        self.children = {} # letter -> DawgState
        self.sorted_keys = None
        self.h = None

    def __eq__(self, DawgState other):
        if self.final != other.final:
            return False

        keys = self.keys()
        other_keys = other.keys()
        if keys != other_keys:
            return False

        for k in keys:
            if self.children[k] is not other.children[k]:
                return False

        return True

    def __hash__(self):
        if self.h is None:
            h = hash_dict(self.children)
            if self.final:
                h *= 2

            self.h = h

        return self.h

    def keys(self):
        if self.sorted_keys is None:
           self.sorted_keys = sorted(self.children.keys())

        return self.sorted_keys

    def has_children(self):
        return len(self.children.keys()) > 0

    def get_child(self, letter):
        return self.children.get(letter)

    def last_child(self):
        keys = self.keys()
        return self.children[keys[-1]] if keys else None

    def set_last_child(self, child):
        self.h = None
        keys = self.keys()
        assert keys
        self.children[keys[-1]] = child

    def add_child(self, letter, child):
        self.sorted_keys = None
        self.h = None
        assert letter not in self.children
        self.children[letter] = child

class Dawg:
    """Directed acyclic word graph used by the main module."""

    def __init__(self, root_final):
        self.root = DawgState(root_final)

    def accepts(self, w):
        state = self.root
        for ch in w:
            state = state.get_child(ch)
            if not state:
                return False

        return state.final

    def track_prefix(self, word):
        prefix = ""
        next_state = self.root
        prev_state = next_state # for empty word
        l = len(word)
        i = 0
        while i < l:
            ch = word[i]
            prev_state = next_state
            next_state = prev_state.get_child(ch)
            if not next_state:
                break
            else:
                prefix += ch

            i += 1

        return ( prefix, prev_state )

class Builder:
    """Directed acyclic word graph builder.

    Use of this class is encapsulated in make_dawg, and the extra
    memory its instances need is released (modulo garbage collection)
    when that function returns.
    """

    def __init__(self, root_final):
        self.dawg = Dawg(root_final)
        self.register = {} # DawgState => (identical) DawgState

    def build(self, words):
        for word in words:
            common_prefix, last_state = self.dawg.track_prefix(word)
            current_suffix = word[len(common_prefix):]
            if last_state.has_children():
                self.replace_or_register(last_state)

            self.add_suffix(last_state, current_suffix)

        self.replace_or_register(self.dawg.root)

    def replace_or_register(self, state):
        assert state
        child = state.last_child()
        if child:
            if child.has_children():
                self.replace_or_register(child)

            q = self.register.get(child)
            if q:
                state.set_last_child(q)
            else:
                self.register[child] = child

    def add_suffix(self, state, suffix):
        assert state
        prev_state = state
        next_state = None
        l = len(suffix)
        i = 1
        for ch in suffix:
            next_state = DawgState(i == l)
            prev_state.add_child(ch, next_state)
            self.register[next_state] = next_state
            prev_state = next_state
            i += 1

def make_dawg(words):
    """Directed acyclic word graph factory."""
    builder = Builder("" in words)
    builder.build(sorted(words))
    return builder.dawg

def main():
    if len(sys.argv) == 1:
        raise Exception("no words")

    words = set(sys.argv[1:])
    if len(words) != len(sys.argv[1:]):
        raise Exception("duplicate words")

    dawg = make_dawg(words)

    neg = ""
    for w in words:
        assert dawg.accepts(w)
        neg += w

    assert not dawg.accepts(neg)

if __name__ == "__main__":
    main()
