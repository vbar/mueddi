//! # Levenshtein-automata imitation model
//!
//! This module probably won't make any sense without reading the
//! paper Fast String Correction with Levenshtein-Automata by Klaus
//! U. Schulz and Stoyan Mihov (called "the paper" below), whose
//! basics it tries to implement.

use std::cell::{Cell, RefCell};
use std::cmp;
use std::collections::{BTreeMap, HashMap};
use std::fmt;
use std::hash::{Hash, Hasher};
use std::rc::Rc;
use superslice::*;

pub const MAX_LEN: usize = 31;

static POWER_MASK: [u32; MAX_LEN] = init_power_mask();

pub type LevenStateRef = Rc<RefCell<LevenState>>;

#[derive(Clone, Copy, Eq, Ord, PartialEq, PartialOrd)]
struct RelPos {
    offset: i16,
    edit: i16
}

#[derive(Clone, Copy, Eq, Ord, PartialEq, PartialOrd)]
struct CharVec {
    bits: u32,
    size: usize
}

struct ReducedUnionPayload {
    pos_list: Vec<RelPos>,
    cached_hash: Cell<u64>
}

#[derive(Clone)]
struct ReducedUnion {
    payload: Rc<RefCell<ReducedUnionPayload>>
}

#[derive(Clone)]
pub struct LevenState {
    base: usize,
    reduced_union: ReducedUnion
}

type TransitionMap = BTreeMap<CharVec, ReducedUnion>;

type LazyMap = HashMap<ReducedUnion, TransitionMap>;

struct LazyTable {
    n: usize,
    state2transition: LazyMap
}

type LazyTableRef = Rc<RefCell<LazyTable>>;

type CacheMap = HashMap<usize, LazyTableRef>;

#[derive(Clone)]
pub struct Cache {
    payload: Rc<RefCell<CacheMap>>
}

struct FacadePayload {
    word: String,
    w: usize,
    lazy_table: LazyTableRef
}

#[derive(Clone)]
pub struct Facade {
    payload: Rc<RefCell<FacadePayload>>
}

impl Hash for RelPos {
    fn hash<H: Hasher>(&self, state: &mut H) {
        let h = self.get_hash();
        h.hash(state);
    }
}

impl fmt::Debug for RelPos {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "+{}#{}", self.offset, self.edit)
    }
}

impl RelPos {
    fn new(o: i16, e: i16) -> Self {
        RelPos {
            offset: o,
            edit: e
        }
    }

    fn get_hash(&self) -> u32 {
        let s = ((MAX_LEN + 1) as i16) * self.offset + self.edit;
        s as u32
    }

    fn subsumes(&self, other: &RelPos) -> bool {
        let r: i16 = other.edit - self.edit;
        if r <= 0 { false } else { i16::abs(other.offset - self.offset) <= r }
    }

    fn subtract(&self, di: i16) -> RelPos {
        RelPos {
            offset: self.offset - di,
            edit: self.edit
        }
    }
}

const fn init_power_mask() -> [u32; MAX_LEN] {
    let mut power_mask: [u32; MAX_LEN] = [0; MAX_LEN];

    let mut i: usize = 0;
    let mut pwr: u32 = 1;
    let mut msk: u32 = 1;
    while i < MAX_LEN {
        power_mask[i] = msk;
        pwr *= 2;
        msk += pwr;
        i += 1;
    }

    power_mask
}

impl fmt::Debug for CharVec {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: 0x{:X}", self.size, self.bits)
    }
}

impl CharVec {
    fn new(b: u32, s: usize) -> Self {
        CharVec {
            bits: b,
            size: s
        }
    }

    fn is_empty(&self) -> bool {
        self.size == 0
    }

    fn subrange(&self, sz: usize, sh: usize) -> CharVec {
        if sz > 0 {
            CharVec {
                bits: (self.bits >> (sh - 1)) & POWER_MASK[sz - 1],
                size: sz
            }
        } else {
            CharVec {
                bits: 0,
                size: 0
            }
        }
    }

    fn has_first_bit_set(&self) -> bool {
        (self.bits & 1) != 0
    }

    fn get_index_of_set_bit(&self) -> i16 {
        assert!(self.bits != 0);

        let mut index: i16 = 1;
        let mut b: u32 = self.bits;
        while (b & 1) == 0 {
            index += 1;
            b /= 2;
        }

        index
    }
}

impl ReducedUnionPayload {
    fn new() -> Self {
        ReducedUnionPayload {
            pos_list: Vec::new(),
            cached_hash: Cell::new(0)
        }
    }
}

fn hash_list(container: &Vec<RelPos>) -> u32 {
    const MOD_ADLER: u32 = 65521;

    let mut a: u32 = 1;
    let mut b: u32 = 0;
    for item in container {
        a = (a + item.get_hash()) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    (b << 16) | a
}

impl PartialEq for ReducedUnion {
    fn eq(&self, other: &Self) -> bool {
        self.payload.borrow().pos_list.eq(&(other.payload.borrow().pos_list))
    }
}

impl Eq for ReducedUnion {}

impl ReducedUnion {
    fn new() -> Self {
        ReducedUnion {
            payload: Rc::new(RefCell::new(ReducedUnionPayload::new()))
        }
    }

    fn get_hash(&self) -> u32 {
        hash_list(&(self.payload.borrow().pos_list))
    }

    fn is_empty(&self) -> bool {
        self.payload.borrow().pos_list.is_empty()
    }

    fn get_raise_level(&self) -> i16 {
        let mn = self.payload.borrow().pos_list.iter().map(|rp| rp.offset).min();
        match mn {
            Some(m) => m,
            None => 0
        }
    }

    fn add(&mut self, rel_pos: &RelPos) {
        let ni = self.payload.borrow().pos_list.lower_bound(rel_pos);
        if (ni < self.payload.borrow().pos_list.len()) && (self.payload.borrow().pos_list[ni] == *rel_pos) {
            return;
        }

        for i in 0..ni {
            if self.payload.borrow().pos_list[i].subsumes(rel_pos) {
                return;
            }
        }

        self.payload.borrow_mut().cached_hash.set(0);
        self.payload.borrow_mut().pos_list.insert(ni, *rel_pos);

        let mut i = ni + 1;
        let mut l = self.payload.borrow().pos_list.len();
        while i < l {
            if rel_pos.subsumes(&(self.payload.borrow().pos_list[i])) {
                self.payload.borrow_mut().pos_list.remove(i);
                l -= 1;
            } else {
                i += 1;
            }
        }
    }

    fn add_unchecked(&mut self, rel_pos: RelPos) {
        assert!(self.payload.borrow().cached_hash.get() == 0);

        let ni = self.payload.borrow().pos_list.lower_bound(&rel_pos);
        assert!((ni == self.payload.borrow().pos_list.len()) || (rel_pos != self.payload.borrow().pos_list[ni]));
        self.payload.borrow_mut().pos_list.insert(ni, rel_pos);
    }

    fn update(&mut self, other: &ReducedUnion) {
        for rp in &(other.payload.borrow().pos_list) {
            self.add(rp);
        }
    }

    fn subtract(&self, di: i16) -> ReducedUnion {
        let red_un = ReducedUnion::new();

        for rp in &(self.payload.borrow().pos_list) {
            red_un.payload.borrow_mut().pos_list.push(rp.subtract(di));
        }

        red_un
    }
}

impl Hash for ReducedUnion {
    fn hash<H: Hasher>(&self, state: &mut H) {
        let mut h = self.payload.borrow().cached_hash.get();
        if h == 0 {
            h = self.get_hash() as u64;
            self.payload.borrow_mut().cached_hash.set(h);
        }

        h.hash(state);
    }
}

fn get_rel_pos_len(n: usize, i: usize, w: usize, e: i16) -> usize {
    assert!((w >= i) && (e >= 0));

    cmp::min(n - (e as usize) + 1, w - i)
}

fn delta_i(rel_pos: &RelPos, char_vec: &CharVec) -> ReducedUnion {
    let mut result = ReducedUnion::new();

    if char_vec.is_empty() {
        result.add_unchecked(RelPos::new(rel_pos.offset, rel_pos.edit + 1));
        return result;
    }

    if char_vec.size == 1 {
        if char_vec.has_first_bit_set() {
            result.add_unchecked(RelPos::new(rel_pos.offset + 1, rel_pos.edit));
        } else {
            result.add_unchecked(RelPos::new(rel_pos.offset, rel_pos.edit + 1));
            result.add_unchecked(RelPos::new(rel_pos.offset + 1, rel_pos.edit + 1));
        }
    } else {
        if char_vec.has_first_bit_set() {
            result.add_unchecked(RelPos::new(rel_pos.offset + 1, rel_pos.edit));
        } else {
            result.add_unchecked(RelPos::new(rel_pos.offset, rel_pos.edit + 1));
            result.add_unchecked(RelPos::new(rel_pos.offset + 1, rel_pos.edit + 1));
            if char_vec.bits != 0 {
                let j = char_vec.get_index_of_set_bit();
                result.add_unchecked(RelPos::new(rel_pos.offset + j, rel_pos.edit + j - 1));
            }
        }
    }

    result
}

fn delta_ii(rel_pos: &RelPos, char_vec: &CharVec) -> ReducedUnion
{
    let mut result = ReducedUnion::new();
    if char_vec.has_first_bit_set() {
        result.add_unchecked(RelPos::new(rel_pos.offset + 1, rel_pos.edit));
    }

    result
}

fn elem_delta(n: usize, i: usize, w: usize, rel_pos: &RelPos, char_vec: &CharVec) -> ReducedUnion {
    let rl = get_rel_pos_len(n, i + rel_pos.offset as usize, w, rel_pos.edit);

    let loc_char_vec = if (rl < char_vec.size) || (rel_pos.offset > 0) {
        char_vec.subrange(rl, 1 + rel_pos.offset as usize)
    } else {
        char_vec.clone()
    };

    if (rel_pos.edit as usize) < n {
        delta_i(rel_pos, &loc_char_vec)
    } else {
        delta_ii(rel_pos, &loc_char_vec)
    }
}

impl LevenState {
    fn new(b: usize, ru: ReducedUnion) -> Self {
        LevenState {
            base: b,
            reduced_union: ru
        }
    }
}

fn make_char_vec(sub_word: &str, letter: char) -> CharVec {
    let mut bits: u32 = 0;
    let mut char_len: usize = 0;
    let mut pwr: u32 = 1;
    for ch in sub_word.chars() {
        if ch == letter {
            bits += pwr;
        }

        char_len += 1;
        pwr *= 2;
    }

    assert!(char_len < 32);

    CharVec::new(bits, char_len)
}

impl LazyTable {
    fn new(n: usize) -> Self {
        let mut table = LazyTable {
            n: n,
            state2transition: LazyMap::new()
        };

        let mut zero = ReducedUnion::new();
        zero.add_unchecked(RelPos::new(0, 0));
        table.state2transition.insert(zero, TransitionMap::new());

        table
    }

    fn get_rel_state_len(&self, i: usize, w: usize) -> usize {
        assert!(w >= i);
        cmp::min(2 * self.n + 1, w - i)
    }

    fn delta(&mut self, pinned_state: &LevenStateRef, w: usize, char_vec: &CharVec) -> ReducedUnion {
        let n = self.n;
        let i = pinned_state.borrow().base;
        let transition: &mut TransitionMap = self.state2transition
            .entry(pinned_state.borrow().reduced_union.clone())
            .or_insert_with(|| TransitionMap::new());

        transition.entry(char_vec.clone())
            .or_insert_with(|| {
                let mut image = ReducedUnion::new();
                for rp in &(pinned_state.borrow().reduced_union.payload.borrow().pos_list) {
                    let ru = elem_delta(n, i, w, rp, char_vec);
                    image.update(&ru);
                }

                image
            }).clone()
    }
}

impl Cache {
    pub fn new() -> Self {
        Cache {
            payload: Rc::new(RefCell::new(CacheMap::new()))
        }
    }
}

impl FacadePayload {
    fn new(word: String, w: usize, lt: LazyTableRef) -> Self {
        FacadePayload {
            word: word,
            w: w,
            lazy_table: lt
        }
    }
}

impl Facade {
    pub fn new(cache: &mut Cache, word: &str, n: usize) -> Self {
        if n > 15 {
            panic!("number of corrections too big for this package");
        }

        let lt = cache.payload.borrow_mut()
            .entry(n).or_insert_with(|| Rc::new(RefCell::new(LazyTable::new(n)))).clone();

        Facade {
            payload: Rc::new(RefCell::new(FacadePayload::new(word.to_string(), word.chars().count(), lt)))
        }
    }

    pub fn is_final(&self, state: &LevenStateRef) -> bool {
        let w = self.payload.borrow().w;
        let n = self.payload.borrow().lazy_table.borrow().n;
        let base = state.borrow().base;

        for rp in &(state.borrow().reduced_union.payload.borrow().pos_list) {
            assert!((rp.edit >= 0) && (rp.offset >= 0));
            if (w + rp.edit as usize) <= (n + base + rp.offset as usize) {
                return true;
            }
        }

        false
    }

    pub fn delta(&self, cur_state: &LevenStateRef, letter: char) -> Option<LevenStateRef> {
        assert!(cur_state.borrow().reduced_union.get_raise_level() == 0);

        let i = cur_state.borrow().base;
        let w = self.payload.borrow().w;
        let word = self.payload.borrow().word.clone();
        let rl = self.payload.borrow().lazy_table.borrow().get_rel_state_len(i, w);
        let mut rel_word = String::new();
        let mut char_idx: usize = 0;
        for ch in word.chars() {
            if char_idx >= i {
                if char_idx < i + rl {
                    rel_word.push(ch);
                } else {
                    break;
                }
            }

            char_idx += 1;
        }

        let char_vec = make_char_vec(&rel_word, letter);
        let image = self.payload.borrow_mut().lazy_table.borrow_mut().delta(cur_state, w, &char_vec);
        if image.is_empty() {
            return None;
        }

        let di = image.get_raise_level();
        assert!(di >= 0);
        let cc = if di != 0 { image.subtract(di) } else { image };
        Some(Rc::new(RefCell::new(LevenState::new(i + di as usize, cc))))
    }
}

pub fn initial_state() -> LevenStateRef
{
    let mut zero_union = ReducedUnion::new();
    zero_union.add_unchecked(RelPos::new(0, 0));
    Rc::new(RefCell::new(LevenState::new(0, zero_union)))
}
