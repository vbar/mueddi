#!/usr/bin/python3

"""Deterministic acyclic finite state automaton for word sets.

AKA Directed acyclic word graph. The algorithm is from Incremental
Construction of Minimal Acyclic Finite-State Automata by Jan Daciuk,
Stoyan Mihov, Bruce W. Watson and Richard E. Watson.
"""

from __future__ import annotations

import sys
from typing import Iterable
from mueddit.struct_hash import hash_dict

class DawgState:
    """Automaton state."""

    def __init__(self, final: bool) -> None:
        self.final = final
        self.children: dict[str, DawgState] = {} # letter -> DawgState
        self.sorted_keys: list[str] | None = None
        self.h: int | None = None

    def __eq__(self, other: object) -> bool:
        assert isinstance(other, DawgState)

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

    def __hash__(self) -> int:
        if self.h is None:
            h = hash_dict(self.children)
            if self.final:
                h *= 2

            self.h = h

        return self.h

    def __str__(self) -> str:
        s = 't' if self.final else 'f'
        s += ' {'
        delim = ' '
        for k, v in self.children.items():
            s += delim
            s += '\''
            s += k
            s += '\': '
            s += str(v)
            delim = ', '

        s += ' }'
        return s;

    def keys(self) -> list[str]:
        if self.sorted_keys is None:
           self.sorted_keys = sorted(self.children.keys())

        return self.sorted_keys

    def has_children(self) -> bool:
        return len(self.children.keys()) > 0

    def get_child(self, letter: str) -> DawgState | None:
        return self.children.get(letter)

    def last_child(self) -> DawgState | None:
        keys = self.keys()
        return self.children[keys[-1]] if keys else None

    def set_last_child(self, child: DawgState) -> None:
        self.h = None
        keys = self.keys()
        assert keys
        self.children[keys[-1]] = child

    def add_child(self, letter: str, child: DawgState) -> None:
        self.sorted_keys = None
        self.h = None
        assert letter not in self.children
        self.children[letter] = child

class Dawg:
    """Directed acyclic word graph used by the main module."""

    def __init__(self, root_final: bool) -> None:
        self.root = DawgState(root_final)

    def __str__(self) -> str:
        return "Dawg: %s" % self.root

    def accepts(self, w: str) -> bool:
        state = self.root
        for ch in w:
            next_state = state.get_child(ch)
            if not next_state:
                return False

            state = next_state

        return state.final

    def track_prefix(self, word: str) -> tuple[str, DawgState]:
        prefix = ""
        next_state = self.root
        prev_state = next_state # for empty word
        l = len(word)
        i = 0
        while i < l:
            ch = word[i]
            prev_state = next_state
            found_state = prev_state.get_child(ch)
            if not found_state:
                break
            else:
                next_state = found_state
                prefix += ch

            i += 1

        return ( prefix, prev_state )

class Builder:
    """Directed acyclic word graph builder.

    Use of this class is encapsulated in make_dawg, and the extra
    memory its instances need is released (modulo garbage collection)
    when that function returns.
    """

    def __init__(self, root_final: bool) -> None:
        self.dawg = Dawg(root_final)
        self.register: dict[DawgState, DawgState] = {} # DawgState => (identical) DawgState

    def build(self, words: Iterable[str]) -> None:
        for word in words:
            common_prefix, last_state = self.dawg.track_prefix(word)
            current_suffix = word[len(common_prefix):]
            if last_state.has_children():
                self.replace_or_register(last_state)

            self.add_suffix(last_state, current_suffix)

        self.replace_or_register(self.dawg.root)

    def replace_or_register(self, state: DawgState) -> None:
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

    def add_suffix(self, state: DawgState, suffix: str) -> None:
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

def make_dawg(words: Iterable[str]) -> Dawg:
    """Directed acyclic word graph factory."""
    builder = Builder("" in words)
    builder.build(sorted(words))
    return builder.dawg

def main() -> None:
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
