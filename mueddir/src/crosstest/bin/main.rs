use argparse::{ArgumentParser, Store, StoreTrue};
use levenshtein::levenshtein;
use std::collections::{BTreeSet, HashSet};
use std::error::Error;
use std::fmt;
use std::fs;
use std::io::{Read, Write};
use std::path::Path;
use std::process;

use mueddi::dawg;
use mueddi::leven;

mod ingest {
    use regex::Regex;
    use std::collections::BTreeSet;
    use std::error::Error;
    use std::fs::File;
    use std::io::{BufRead, BufReader};
    use std::path::Path;

    pub fn make_test_dict(input_path: &Path) -> Result<BTreeSet<String>, Box<dyn Error>> {
        let unword = Regex::new("[\r\n\t .?!,;:\"'()\\[\\]{}&*#$@_]").unwrap();
        let mut dictionary: BTreeSet<String> = BTreeSet::new();

        let f = File::open(input_path)?;
        for lnp in BufReader::new(f).lines() {
            let ln = lnp?;
            for word in unword.split(&ln) {
                if !word.is_empty() {
                    dictionary.insert(word.to_string());
                }
            }
        }

        Ok(dictionary)
    }
}

#[derive(Debug)]
struct TestError(String);

impl fmt::Display for TestError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl Error for TestError {}

fn test_independent<W: Write>(seen: &str, n: usize, dictionary: &BTreeSet<String>, dawg: &dawg::Dawg, cache: &mut leven::Cache, writer: &mut csv::Writer<W>) -> Result<(), Box<dyn Error>> {
    let mut external: HashSet<String> = HashSet::new();
    for correct in dictionary {
        if levenshtein(seen, correct) <= n {
            external.insert(correct.to_string());
        }
    }

    let mut row: Vec<String> = Vec::new();
    row.push(seen.to_string());

    let mut internal: HashSet<String> = HashSet::new();
    let it = mueddi::ResultIterator::new(seen, n, dawg, cache);
    for found in it {
        row.push(found.clone());
        internal.insert(found);
    }

    writer.write_record(&row)?;

    if external == internal {
        Ok(())
    } else {
        Err(Box::new(TestError(format!("results for {} differ", seen))))
    }
}

fn test_repeat<R: Read>(seen: &str, n: usize, dawg: &dawg::Dawg, cache: &mut leven::Cache, records: &mut csv::StringRecordsIter<R>) -> Result<(), Box<dyn Error>> {
    let item = match records.next() {
        Some(result) => result,
        None => {
            return Err(Box::new(TestError(String::from("missing row"))));
        }
    };

    let record = item?;

    match record.get(0) {
        Some(prev_seen) => {
            if prev_seen != seen {
                return Err(Box::new(TestError(String::from("result row start mismatch"))));
            }
        },
        None => {
            return Err(Box::new(TestError(String::from("empty row"))));
        }
    }

    let mut i = 1;
    let it = mueddi::ResultIterator::new(seen, n, dawg, cache);
    for found in it {
        match record.get(i) {
            Some(prev_match) => {
                if prev_match != found {
                    return Err(Box::new(TestError(String::from("result row mismatch"))));
                }
            },
            None => {
                return Err(Box::new(TestError(String::from("row missing matches"))));
            }
        }

        i += 1;
    }

    Ok(())
}

fn run(n: usize, input: &str, result: &str, single_mode: bool) -> Result<(), Box<dyn Error>> {
    let input_path = fs::canonicalize(input)?;
    let result_path = Path::new(&result);
    let single_num = if single_mode { 1 } else { 0 };

    let input_str = match input_path.to_str() {
        Some(is) => is,
        None => {
            return Err(Box::new(TestError(format!("cannot convert {}", input))));
        }
    };

    let dictionary = ingest::make_test_dict(&input_path)?;
    let mut dd = dictionary.clone();
    let mut words: Vec<String> = dictionary.iter().cloned().collect();
    let mut dawg = dawg::make_dawg_impl(&mut words);
    let mut cache = leven::Cache::new();

    if !result_path.exists() {
        let mut writer = csv::WriterBuilder::new()
            .delimiter(b'\t')
            .flexible(true)
            .from_path(result_path)?;

        let mut meta: Vec<String> = Vec::new();
        meta.push(input_str.to_string());
        meta.push(n.to_string());
        meta.push(single_num.to_string());
        writer.write_record(&meta)?;

        let mut last_word = String::new();
        let mut first = true;
        for tword in &dictionary {
            eprintln!("{}...", tword);

            if !single_mode {
                dd.remove(tword);

                if first {
                    first = false;
                } else {
                    dd.insert(last_word);
                }

                let mut other_words: Vec<String> = dd.iter().cloned().collect();
                dawg = dawg::make_dawg_impl(&mut other_words);
                last_word = tword.to_string();
            }

            test_independent(tword, n, &dd, &dawg, &mut cache, &mut writer)?;
        }

        writer.flush()?;
    } else {
        let mut reader = csv::ReaderBuilder::new()
            .delimiter(b'\t')
            .has_headers(false)
            .flexible(true)
            .from_path(result_path)?;
        let mut records = reader.records();

        let first_item = match records.next() {
            Some(result) => result,
            None => {
                return Err(Box::new(TestError(String::from("empty input"))));
            }
        };

        let first_record = first_item?;
        if first_record.len() != 3 {
            return Err(Box::new(TestError(String::from("three-column header expected"))));
        }

        match first_record.get(0) {
            Some(prev_input) => {
                if prev_input != input_str {
                    return Err(Box::new(TestError(String::from("input changed"))));
                }
            },
            None => {
                panic!("header inconsistent");
            }
        }

        match first_record.get(1) {
            Some(prev_tolerance) => {
                if prev_tolerance != n.to_string() {
                    return Err(Box::new(TestError(String::from("tolerance changed"))));
                }
            },
            None => {
                panic!("header inconsistent");
            }
        }

        match first_record.get(2) {
            Some(prev_single) => {
                if prev_single != single_num.to_string() {
                    return Err(Box::new(TestError(String::from("single mode changed"))));
                }
            },
            None => {
                panic!("header inconsistent");
            }
        }

        let mut last_word = String::new();
        let mut first = true;
        for tword in &dictionary {
            eprintln!("{}...", tword);

            if !single_mode {
                dd.remove(tword);

                if first {
                    first = false;
                } else {
                    dd.insert(last_word);
                }

                let mut other_words: Vec<String> = dd.iter().cloned().collect();
                dawg = dawg::make_dawg_impl(&mut other_words);
                last_word = tword.to_string();
            }

            test_repeat(tword, n, &dawg, &mut cache, &mut records)?;
        }
    }

    Ok(())
}

fn main() {
    let mut n = 1;
    let mut input = String::new();
    let mut result = String::from("result.tsv");
    let mut single_mode = false;

    {
        let mut parser = ArgumentParser::new();
        parser.set_description("MUlti-word EDit DIstance test");
        parser.refer(&mut n)
            .add_option(&["-t", "--tolerance"], Store, "max allowed number of edits");
        parser.refer(&mut input)
            .add_argument("input", Store, "input file path");
        parser.refer(&mut result)
            .add_option(&["-r", "--result"], Store, "result of a previous run");
        parser.refer(&mut single_mode)
            .add_option(&["-s", "--single-dict"], StoreTrue, "include tested word in the dictionary");
        parser.parse_args_or_exit();
    }

    if (n <= 0) || ( n > 15) {
        eprintln!("crosstest error: max allowed number of edits must be a positive number less than 16");
        process::exit(1);
    }

    if input.is_empty() {
        eprintln!("crosstest error: input file must be specified");
        process::exit(1);
    }

    if let Err(err) = run(n, &input, &result, single_mode) {
        eprintln!("crosstest error: {}", err);
        process::exit(1);
    }
}
