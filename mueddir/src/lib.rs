//! # MUlti-word EDit DIstance

//! Given a dictionary of expected words and a possibly misspelled
//! word, computes the subset of the dictionary within a specified
//! Levenshtein distance from the word.

use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

pub mod dawg;
pub mod leven;

pub use self::dawg::{Dawg, make_dawg_impl, Words};
pub use self::leven::Cache;

#[derive(Clone)]
struct QueueItem {
    candidate: String,
    dawg_state: dawg::DawgStateRef,
    leven_state: leven::LevenStateRef
}

type ResultQueue = VecDeque<QueueItem>;

struct IteratorPayload {
    facade: leven::Facade,
    queue: ResultQueue,
    current: String,
    valid: bool
}

/// Standard iterator interface.
///
/// # Example
///
/// ```
/// let mut cache = mueddi::Cache::new();
/// let data = vec![ "this", "that", "other" ];
///
/// let mut v: Vec<String> = data.iter().map(|s| String::from(*s)).collect();
/// let dawg = mueddi::make_dawg_impl(&mut v);
/// let it = mueddi::ResultIterator::new("the", 2, &dawg, &mut cache);
/// for s in it {
///     println!("{}", s);
/// }
/// ```
#[derive(Clone)]
pub struct ResultIterator {
    payload: Rc<RefCell<IteratorPayload>>
}

impl QueueItem {
    fn new(v: String, q: dawg::DawgStateRef, m: leven::LevenStateRef) -> Self {
        QueueItem {
            candidate: v,
            dawg_state: q,
            leven_state: m
        }
    }
}

impl IteratorPayload {
    fn new(seen: &str, n: usize, dawg: &dawg::Dawg, cache: &mut leven::Cache) -> Self {
        let mut queue = ResultQueue::new();
        queue.push_back(QueueItem::new(String::new(), dawg.get_root(), leven::initial_state()));

        IteratorPayload {
            facade: leven::Facade::new(cache, seen, n),
            queue: queue,
            current: String::new(),
            valid: false
        }
    }

    fn advance(&mut self) {
        self.valid = false;
        while !self.valid {
            if let Some(item) = self.queue.pop_front() {
                if item.dawg_state.borrow().is_final() && self.facade.is_final(&(item.leven_state)) {
                    self.current = item.candidate.clone();
                    self.valid = true;
                }

                for (x, q1) in item.dawg_state.borrow().get_children().iter() {
                    if let Some(mp) = self.facade.delta(&(item.leven_state), *x) {
                        let mut v1: String = item.candidate.clone();
                        v1.push(*x);
                        self.queue.push_back(QueueItem::new(v1, Rc::clone(q1), mp));
                    }
                }
            } else {
                break;
            }
        }
    }
}

impl ResultIterator {
    pub fn new(seen: &str, n: usize, dawg: &dawg::Dawg, cache: &mut leven::Cache) -> Self {
        let mut ip = IteratorPayload::new(seen, n, dawg, cache);
        ip.advance();

        ResultIterator {
            payload: Rc::new(RefCell::new(ip))
        }
    }
}

impl Iterator for ResultIterator {
    type Item = String;

    fn next(&mut self) -> Option<Self::Item> {
        if !self.payload.borrow().valid {
            return None;
        }

        let current = self.payload.borrow().current.clone();
        self.payload.borrow_mut().advance();
        Some(current)
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;
    use super::*;

    #[test]
    fn test_initial_final() {
        let mut cache = leven::Cache::new();
        let data = vec![ "", "a" ];

        let mut v: Vec<String> = Vec::new();
        for s in &data {
            v.push(String::from(*s));
        }

        let dawg = dawg::make_dawg_impl(&mut v);
        let it = ResultIterator::new("b", 1, &dawg, &mut cache);
        let res: HashSet<String> = it.collect();

        assert_eq!(2, res.len());
        for s in data {
            assert!(res.contains(s));
        }
    }

    #[test]
    fn test_foo() {
        let mut cache = leven::Cache::new();
        let data = vec![ "foo", "bar" ];

        let mut v: Vec<String> = Vec::new();
        for s in &data {
            v.push(String::from(*s));
        }

        let dawg = dawg::make_dawg_impl(&mut v);

        {
            let it = ResultIterator::new("baz", 1, &dawg, &mut cache);
            let res: HashSet<String> = it.collect();
            assert_eq!(1, res.len());
            assert!(res.contains("bar"));
        }

        {
            let it = ResultIterator::new("baz", 2, &dawg, &mut cache);
            let res: HashSet<String> = it.collect();
            assert_eq!(1, res.len());
            assert!(res.contains("bar"));
        }
    }

    #[test]
    fn test_this() {
        let mut cache = leven::Cache::new();
        let data = vec![ "this", "that", "other" ];

        let mut v: Vec<String> = Vec::new();
        for s in &data {
            v.push(String::from(*s));
        }

        let dawg = dawg::make_dawg_impl(&mut v);

        {
            let mut it = ResultIterator::new("the", 1, &dawg, &mut cache);
            assert_eq!(None, it.next());
        }

        {
            let it = ResultIterator::new("the", 2, &dawg, &mut cache);
            let res: HashSet<String> = it.collect();

            assert_eq!(3, res.len());
            for s in data {
                assert!(res.contains(s));
            }
        }
    }

    #[test]
    fn test_long_head() {
        let mut cache = leven::Cache::new();
        let single = "abtrbtz";

        let mut v: Vec<String> = Vec::new();
        v.push(String::from(single));

        let dawg = dawg::make_dawg_impl(&mut v);
        let it = ResultIterator::new("abtrtz", 1, &dawg, &mut cache);
        let res: HashSet<String> = it.collect();
        assert_eq!(1, res.len());
        assert!(res.contains(single));
    }

    #[test]
    fn test_tolerance() {
        let mut cache = leven::Cache::new();
        let data = vec![ "meter", "otter", "potter" ];

        let mut v: Vec<String> = Vec::new();
        for s in &data {
            v.push(String::from(*s));
        }

        let dawg = dawg::make_dawg_impl(&mut v);

        {
            let mut it = ResultIterator::new("mutter", 1, &dawg, &mut cache);
            assert_eq!(None, it.next());
        }

        {
            let it = ResultIterator::new("mutter", 2, &dawg, &mut cache);
            let res: HashSet<String> = it.collect();

            assert_eq!(3, res.len());
            for s in data {
                assert!(res.contains(s));
            }
        }
    }

    #[test]
    fn test_binary() {
        let mut cache = leven::Cache::new();
        let data = vec![ "ababa", "babab" ];

        let mut v: Vec<String> = Vec::new();
        for s in &data {
            v.push(String::from(*s));
        }

        let dawg = dawg::make_dawg_impl(&mut v);
        let it = ResultIterator::new("abba", 3, &dawg, &mut cache);
        let res: HashSet<String> = it.collect();

        assert_eq!(2, res.len());
        for s in data {
            assert!(res.contains(s));
        }
    }
}
