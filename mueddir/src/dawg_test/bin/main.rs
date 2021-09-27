use std::collections::HashSet;
use std::env;
use std::iter::FromIterator;

fn main() {
    let mut it = env::args();
    it.next();
    let mut words: mueddi::Words = it.collect();
    if words.is_empty() {
        panic!("no words");
    }

    let word_set: HashSet<String> = HashSet::from_iter(words.iter().cloned());
    if word_set.len() < words.len() {
        panic!("duplicate words");
    }

    let dawg = mueddi::make_dawg_impl(&mut words);

    let mut neg = String::from("~");
    for w in words {
        if !dawg.accepts(&w) {
            panic!("does not accept {}", w);
        }

        neg.push_str(&w);
    }

    if dawg.accepts(&neg) {
        panic!("accepts {}", neg);
    }
}
