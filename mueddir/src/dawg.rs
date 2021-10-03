//! # Deterministic acyclic finite state automaton for word sets
//!
//! AKA Directed acyclic word graph. The algorithm is from Incremental
//! Construction of Minimal Acyclic Finite-State Automata by Jan
//! Daciuk, Stoyan Mihov, Bruce W. Watson and Richard E. Watson.

use std::cell::RefCell;
use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt;
use std::rc::Rc;

pub type DawgStateRef = Rc<RefCell<DawgState>>;

pub type Words = Vec<String>;

pub struct DawgState {
    finl: bool,
    children: BTreeMap<char, DawgStateRef>
}

#[derive(Debug)]
pub struct Dawg {
    root: DawgStateRef
}

struct Builder {
    dawg: Dawg,
    register: BTreeSet<DawgStateRef>
}

impl DawgState {
    fn new(finl: bool) -> Self {
        DawgState {
            finl: finl,
            children: BTreeMap::new()
        }
    }

    pub fn get_children(&self) -> &BTreeMap<char, DawgStateRef> {
        &(self.children)
    }

    pub fn is_final(&self) -> bool {
        self.finl
    }

    fn has_children(&self) -> bool {
        !self.children.is_empty()
    }

    fn get_child(&self, letter: char) -> Option<DawgStateRef> {
        match self.children.get(&letter) {
            Some(s) => Some(Rc::clone(&s)),
            None => None
        }
    }

    fn last_child(&self) -> Option<DawgStateRef> {
        if self.children.is_empty() {
            None
        } else {
            let last = self.children.iter().next_back().unwrap().1;
            Some(Rc::clone(&last))
        }
    }

    fn set_last_child(&mut self, child: DawgStateRef) {
        assert!(!self.children.is_empty());
        *self.children.iter_mut().next_back().unwrap().1 = child;
    }

    fn add_child(&mut self, letter: char, child: DawgStateRef) {
        if let Some(_old) = self.children.insert(letter, child) {
            panic!("child for {} already exists", letter);
        }
    }

    fn compare(&self, other: &Self) -> std::cmp::Ordering {
        if self.finl != other.finl {
            if self.finl {
                return Ordering::Greater;
            } else {
                return Ordering::Less;
            }
        }

        let k = self.children.keys().len();
        let l = other.children.keys().len();
        if k != l {
            if k > l {
                return Ordering::Greater;
            } else {
                return Ordering::Less;
            }
        }

        for (sit, oit) in self.children.iter().zip(other.children.iter()) {
            if sit.0 != oit.0 {
                if sit.0 > oit.0 {
                    return Ordering::Greater;
                } else {
                    return Ordering::Less;
                }
            }

            let sp = sit.1.as_ptr();
            let op = oit.1.as_ptr();
            if sp != op {
                if sp > op {
                    return Ordering::Greater;
                } else {
                    return Ordering::Less;
                }
            }
        }

        return Ordering::Equal;
    }
}

impl fmt::Debug for DawgState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", if self.finl { 't' } else { 'f' })?;
        write!(f, "{}", " {")?;

        let mut delim = " ";
        for (k, v) in self.children.iter() {
            write!(f, "{}\'{}\': ", delim, k)?;
            v.fmt(f)?;
            delim = ", ";
        }

        write!(f, "{}", " }")
    }
}

impl PartialEq for DawgState {
    fn eq(&self, other: &Self) -> bool {
        if self.finl != other.finl {
            return false
        }

        if self.children.keys().len() != other.children.keys().len() {
            return false;
        }

        for (sit, oit) in self.children.iter().zip(other.children.iter()) {
            if sit.0 != oit.0 {
                return false;
            }

            if sit.1.as_ptr() != oit.1.as_ptr() {
                return false;
            }
        }

        return true
    }
}

impl Eq for DawgState {}

impl PartialOrd for DawgState {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.compare(&other))
    }
}

impl Ord for DawgState {
    fn cmp(&self, other: &Self) -> Ordering {
        self.compare(other)
    }
}

impl Dawg {
    fn new(root_final: bool) -> Self {
        Dawg {
            root: Rc::new(RefCell::new(DawgState::new(root_final)))
        }
    }

    pub fn get_root(&self) -> DawgStateRef {
        Rc::clone(&(self.root))
    }

    pub fn accepts(&self, w: &str) -> bool {
        let mut state = Rc::clone(&(self.root));
        for ch in w.chars() {
            let child = state.borrow().get_child(ch);
            match child {
                Some(s) => { state = s; },
                None => { return false; }
            }
        }

        let last = state.borrow();
        last.finl
    }

    fn track_prefix(&self, word: &str) -> ( String, DawgStateRef ) {
        let mut prefix = String::new();
        let mut next_state = Rc::clone(&(self.root));
        let mut prev_state = Rc::clone(&next_state);

        for ch in word.chars() {
            prev_state = next_state;
            match prev_state.borrow().get_child(ch) {
                Some(s) => {
                    next_state = s;
                    prefix.push(ch);
                },
                None => {
                    break;
                }
            }
        }

        (prefix, prev_state)
    }
}

impl Builder {
    fn new(root_final: bool) -> Self {
        Builder {
            dawg: Dawg::new(root_final),
            register: BTreeSet::new()
        }
    }

    fn build(&mut self, words: &Vec<String>) {
        for word in words {
            let (common_prefix, last_state) = self.dawg.track_prefix(word);
            let current_suffix = &word[common_prefix.len()..];
            if last_state.borrow().has_children() {
                self.replace_or_register(&last_state);
            }

            self.add_suffix(&last_state, current_suffix);
        }

        let root = Rc::clone(&(self.dawg.root));
        self.replace_or_register(&root);
    }

    fn replace_or_register(&mut self, state: &DawgStateRef) {
        let last = state.borrow().last_child();
        if let Some(child) = last {
            if child.borrow().has_children() {
                self.replace_or_register(&child);
            }

            if let Some(q) = self.register.get(&child) {
                state.borrow_mut().set_last_child(Rc::clone(&q));
            } else {
                self.register.insert(child);
            }
        }
    }

    fn add_suffix(&mut self, state: &DawgStateRef, suffix: &str) {
        let mut prev_state = Rc::clone(state);
        let mut iter = suffix.chars().peekable();
        while let Some(ch) = iter.next() {
            let finl = match iter.peek() {
                Some(_c) => false,
                None => true
            };

            let next_state = Rc::new(RefCell::new(DawgState::new(finl)));
            prev_state.borrow_mut().add_child(ch, Rc::clone(&next_state));
            self.register.insert(Rc::clone(&next_state));
            prev_state = next_state;
        }
    }
}

pub fn make_dawg_impl(words: &mut Words) -> Dawg {
    words.sort();

    let mut builder = Builder::new(words.is_empty() || words[0].is_empty());
    builder.build(words);
    builder.dawg
}
