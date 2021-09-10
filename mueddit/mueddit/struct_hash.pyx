# cython: language_level=3

"""Naive Adler-32 hash computation for lists and dicts.

Specialized for the needs of Deterministic acyclic finite state
automata and Levenshtein automata in this package.
"""

MOD_ADLER = 65521

def hash_list(lst):
    """Hash (sorted) list, delegating to its members.

    Not suitable for general iterables (like set) because it expects
    same items in all lists to be in the same order.
    """
    a = 1
    b = 0
    for it in lst:
        a = (a + hash(it)) % MOD_ADLER
        b = (b + a) % MOD_ADLER

    return (b << 16) | a;

def hash_dict(d):
    """Hash dictionary with single-character keys and object values.

    Values are hashed by id, i.e. for equality using the is operator.
    """
    a = 1
    b = 0
    keys = sorted(d.keys())
    for k in keys:
        # key
        a = (a + ord(k)) % MOD_ADLER
        b = (b + a) % MOD_ADLER

        # value
        a = (a + id(d[k])) % MOD_ADLER
        b = (b + a) % MOD_ADLER

    return (b << 16) | a;
