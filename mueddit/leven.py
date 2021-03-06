"""Levenshtein-automata imitation model.

This module probably won't make any sense without reading the paper
Fast String Correction with Levenshtein-Automata by Klaus U. Schulz
and Stoyan Mihov (called "the paper" below), whose basics it tries to
implement.
"""

from mueddit.struct_hash import hash_list
from bisect import bisect_left, insort_left
import logging

# if somebody wants n > 15, they'll need a custom implementation
# anyway...
MAX_LEN = 31

log = logging.getLogger('mueddi')

class RelPos:
    """Position ("i^#e"), except it's relative to some outside i."""

    def __init__(self, o, e):
        assert (o >= 0) and (e >= 0)
        self.offset = o
        self.edit = e

    def __eq__(self, other):
        assert isinstance(other, RelPos)

        return (self.edit == other.edit) and (self.offset == other.offset)

    def __lt__(self, other):
        assert isinstance(other, RelPos)

        return (self.edit < other.edit) or ((self.edit == other.edit) and (self.offset < other.offset))

    def __hash__(self):
        return (MAX_LEN + 1) * self.offset + self.edit

    def __str__(self):
        return "+%d#%d" % (self.offset, self.edit)

    def __repr__(self):
        return "'%s'" % self.__str__()

    def subsumes(self, other):
        """Subsumption relation from the paper.

        Works even for relative positions, as long as they're relative
        to the same boundary.
        """
        assert isinstance(other, RelPos)
        r = other.edit - self.edit
        return False if r <= 0 else abs(other.offset - self.offset) <= r

    def subtract(self, di):
        """Returns this position for a higher base."""
        assert (di > 0) and (self.offset >= di)
        return RelPos(self.offset - di, self.edit)

# conceptually a class attribute of CharVec
power_mask = []

def init_power_mask():
    i = 0
    pwr = 1
    msk = 1
    while i < MAX_LEN:
        power_mask.append(msk)
        pwr *= 2
        msk += pwr
        i += 1

init_power_mask()

class CharVec:
    """Characteristic (_not_ character) vector."""

    def __init__(self, b, s):
        """Initializes from bitmask and size.

        Lowest bitmask bit is for leftmost pos (with index 1); all
        bits beyond size must be 0.
        """
        assert (b >= 0) and (s >= 0) and (s <= MAX_LEN)
        self.bits = b
        self.size = s

    def __eq__(self, other):
        assert isinstance(other, CharVec)

        return (self.size == other.size) and (self.bits == other.bits)

    def __hash__(self):
        return (3 + self.bits) * self.size

    def __str__(self):
        return "%d: %s" % (self.size, hex(self.bits))

    def is_empty(self):
        """Empty (size 0) vector has a limited interface (see below)."""
        return not self.size

    def subrange(self, sz, sh):
        """Returns sz bits of this vector, starting from bit sh."""
        assert (0 <= sz) and (sz <= self.size) and (1 <= sh) and (sh <= self.size + 1)
        return CharVec((self.bits >> (sh - 1)) & power_mask[sz - 1], sz) if sz else CharVec(0, 0)

    def has_first_bit_set(self):
        """Has this vector its first bit set?

        Returns false for empty vectors (which do not have a first bit
        at all).
        """
        return self.bits & 1

    def get_index_of_set_bit(self):
        """The lowest index (1-based) of a set bit.

        Returns None when there isn't any such index. Only callable
        for non-empty vectors.
        """
        assert self.size > 0
        if not self.bits:
            return None

        index = 1
        b = self.bits
        while not (b & 1):
            index += 1
            b //= 2

        return index

class ReducedUnion:
    """Set of RelPos objects.

    Possibly empty, no member subsumes any other. All contained
    positions must be relative to the same boundary.
    """

    def __init__(self):
        self.pos_list = [] # ordered list of unique RelPos

    def __eq__(self, other):
        assert isinstance(other, ReducedUnion)

        return self.pos_list == other.pos_list

    def __hash__(self):
        return hash_list(self.pos_list)

    def __str__(self):
        return str(self.pos_list)

    def __iter__(self):
        return iter(self.pos_list)

    def is_empty(self):
        return not len(self.pos_list)

    def get_raise_level(self):
        """The maximum that can be subtracted from this set.

        When it returns 0, the set is already in base state and
        there's no need to subtract anything.
        """
        if not len(self.pos_list):
            return 0

        return min((rp.offset for rp in self.pos_list))

    def add(self, rel_pos):
        """Add a position, preserving the "no subsumption" invariant.

        Caller must ensure the position is relative to the same
        boundary as the already included positions.
        """
        assert rel_pos

        ni = bisect_left(self.pos_list, rel_pos)
        if (ni < len(self.pos_list)) and (rel_pos == self.pos_list[ni]):
            return

        for i in range(ni):
            if self.pos_list[i].subsumes(rel_pos):
                return

        self.pos_list.insert(ni, rel_pos)

        i = ni + 1
        l = len(self.pos_list)
        while i < l:
            if rel_pos.subsumes(self.pos_list[i]):
                del self.pos_list[i]
                l -= 1
            else:
                i += 1

    def add_unchecked(self, rel_pos):
        """Add a position, not checking the "no subsumption" invariant.

        Caller must know the position isn't in the set yet and doesn't
        break the invariant (i.e. because they add a fixed set into an
        initially empty object), and also ensure the position is
        relative to the same boundary as the already included
        positions.
        """
        assert rel_pos
        insort_left(self.pos_list, rel_pos)

    def update(self, other):
        """Add positions from another set.

        Caller must ensure both sets are relative to the same boundary.
        """
        assert isinstance(other, ReducedUnion)

        for rp in other:
            self.add(rp)

    def subtract(self, di):
        """Recompute this set for a higher base."""
        assert (di > 0) and len(self.pos_list)

        red_un = ReducedUnion()
        for rp in self.pos_list:
            # rebasing maps valid state to valid state - no need to check
            red_un.pos_list.append(rp.subtract(di))

        return red_un

class Elementary:
    """Elementary transitions.

    Not much of an object - the only state it keeps is n."""

    def __init__(self, n):
        assert n > 0
        self.n = n

    def elem_delta(self, i, w, rel_pos, char_vec):
        """Elementary transition for a position and a characteristic vector.

        Note that the position is absolute - caller must supply both
        relative position and its base. The supplied characteristic
        vector is relevant to caller's LevenState and is sub-ranged to
        position-relevant inside the function.
        """
        log.debug("enter Elementary.elem_delta(%d, %d, %s, %s)" % (i, w, rel_pos, char_vec))
        assert rel_pos and (0 <= rel_pos.edit) and (rel_pos.edit <= self.n) and char_vec
        rl = self.get_rel_pos_len(i + rel_pos.offset, w, rel_pos.edit)
        assert rl <= char_vec.size
        loc_char_vec = char_vec.subrange(rl, 1 + rel_pos.offset) if (rl < char_vec.size) or (rel_pos.offset > 0) else char_vec
        log.debug("loc_char_vec = %s" % loc_char_vec)
        return self.deltaI(rel_pos, loc_char_vec) if rel_pos.edit < self.n else self.deltaII(rel_pos, loc_char_vec)

    def deltaI(self, rel_pos, char_vec):
        """Part I of Table 4.1 of the paper."""
        result = ReducedUnion()
        if char_vec.is_empty():
            result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1))
            return result

        if char_vec.size == 1:
            if char_vec.has_first_bit_set():
                result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit))
            else:
                result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1))
                result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit + 1))
        else:
            if char_vec.has_first_bit_set():
                result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit))
            else:
                result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1))
                result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit + 1))
                j = char_vec.get_index_of_set_bit()
                if j is not None:
                    assert j > 1
                    result.add_unchecked(RelPos(rel_pos.offset + j, rel_pos.edit + j - 1))

        return result

    def deltaII(self, rel_pos, char_vec):
        """Part II of Table 4.1 of the paper."""
        result = ReducedUnion()
        if char_vec.has_first_bit_set():
            result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit))

        return result

    def get_rel_pos_len(self, i, w, e):
        """Length of the relevant subword for the specified position."""
        assert (0 <= i) and (i <= w) and (0 <= e) and (e <= self.n)
        return min(self.n - e + 1, w - i)

class LevenState:
    """Automaton state.

    Named pair of ReducedUnion and its base. The union must not be
    empty - the empty (failure) state is represented by None.
    """

    def __init__(self, b, ru):
        assert (b >= 0) and ru and not ru.is_empty()
        self.base = b
        self.reduced_union = ru

    def __str__(self):
        return "%d: %s" % (self.base, self.reduced_union)

class LazyTable(Elementary):
    """State transitions, computed on demand and cached.

    Memory requirements of objects of this class increase
    exponentially with n - in other words, they aren't unbounded
    (because n is bounded) but can be high.
    """

    def __init__(self, n):
        Elementary.__init__(self, n)
        zero = ReducedUnion()
        zero.add_unchecked(RelPos(0, 0))
        # filled lazily, but we'll certainly need the initial position
        self.state2transition = { zero: {} } # ReducedUnion -> { CharVec -> ReducedUnion }

    def get_rel_state_len(self, i, w):
        """Length of the relevant subword for the specified boundary."""
        return min(2 * self.n + 1, w - i)

    def make_char_vec(self, sub_word, letter):
        """Converts (relevant) sub-word to its characteristic vector."""
        log.debug("enter make_char_vec(\"%s\", '%s')" % (sub_word, letter))
        char_vec = CharVec(0, len(sub_word))
        i = 0
        pwr = 1
        while i < char_vec.size:
            if sub_word[i] == letter:
                char_vec.bits += pwr

            i += 1
            pwr *= 2

        log.debug("exit make_char_vec: %s" % char_vec)
        return char_vec

    def delta(self, pinned_state, w, char_vec):
        log.debug("enter LazyTable.delta(%s, %d, %s)" % (pinned_state, w, char_vec))
        i = pinned_state.base
        red_un = pinned_state.reduced_union
        if red_un not in self.state2transition:
            self.state2transition[red_un] = {}

        transition = self.state2transition[red_un]
        if char_vec not in transition:
            image = ReducedUnion()
            for rp in red_un:
                image.update(self.elem_delta(i, w, rp, char_vec))

            transition[char_vec] = image

        log.debug("exit LazyTable.delta: %s" % transition[char_vec])
        return transition[char_vec]

class Facade:
    """Interface to the Levenshtein automaton used by the main module.

    Note this isn't quite the whole automaton (doesn't have the full
    list of states, for example) - just enough to get matches.
    """

    cache = {} # n -> LazyTable(n)

    def __init__(self, word, n):
        if n <= 0:
            raise Exception("number of corrections must be positive")

        if n > 15:
            raise Exception("%d corrections too big for this package" % n)

        self.word = word
        self.w = len(word)

        lazy_table = self.cache.get(n)
        if not lazy_table:
            lazy_table = LazyTable(n)
            self.cache[n] = lazy_table

        self.lazy_table = lazy_table

    def initial_state(self):
        """Provides the initial { 0^#0 }."""
        zero_pos = RelPos(0, 0)
        zero_union = ReducedUnion()
        zero_union.add_unchecked(zero_pos)
        return LevenState(0, zero_union)

    def delta(self, cur_state, letter):
        """State transition."""
        log.debug("enter Facade.delta(%s, %s)" % (cur_state, letter))
        assert cur_state and cur_state.reduced_union.get_raise_level() == 0
        i = cur_state.base
        rl = self.lazy_table.get_rel_state_len(i, self.w)
        rel_word = self.word[i:i+rl]
        char_vec = self.lazy_table.make_char_vec(rel_word, letter)
        image = self.lazy_table.delta(cur_state, self.w, char_vec)
        if image.is_empty():
            return None

        di = image.get_raise_level()
        cc = image.subtract(di) if di else image
        log.debug("rebased: %d, %s" % (di, cc))
        return LevenState(cur_state.base + di, cc)

    def is_final(self, state):
        """Final state check."""
        for rp in state.reduced_union:
            i = state.base + rp.offset
            if self.w - i <= self.n - rp.edit:
                return True

        return False

    @property
    def n(self):
        return self.lazy_table.n
