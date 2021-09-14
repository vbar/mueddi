#include "csv2.hpp"
#include "ingest.hh"
#include "levenshtein.hh"
#include "mueddi.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

using namespace mueddi;

using std::filesystem::path;

using CsvReader = csv2::Reader<csv2::delimiter<'\t'>>;
using CsvWriter = csv2::Writer<csv2::delimiter<'\t'>>;

class Options
{
public:
    int tolerance;
    std::string result;
    bool single_dict;
    std::string input;

    Options();

    bool parse(char *argv[]);

private:
    bool explicit_flag[4];

    bool set_explicit_flag(int index);
};

Options::Options():
    tolerance(1),
    result("result.tsv"),
    single_dict(false),
    input()
{
    for (size_t i = 0; i < sizeof(explicit_flag) / sizeof(explicit_flag[0]); ++i) {
        explicit_flag[i] = false;
    }
}

bool Options::parse(char *argv[])
{
    long long_tolerance;
    char *endptr = nullptr;
    int index = -1;

    ++argv;
    while (*argv) {
        const char *a = *argv;
        switch (index) {
            case 0:
                long_tolerance = strtol(a, &endptr, 10);
                if ((long_tolerance <= 0) || *endptr) {
                    return false;
                }

                tolerance = static_cast<int>(long_tolerance);
                if (tolerance != long_tolerance) {
                    return false;
                }

                index = -1;
                break;

            case 1:
                result = a;
                index = -1;
                break;

            default:
                if (!strcmp(a, "--tolerance") || !strcmp(a, "-t")) {
                    index = 0;
                    if (!set_explicit_flag(index)) {
                        return false;
                    }
                } else if (!strcmp(a, "--result") || !strcmp(a, "-r")) {
                    index = 1;
                    if (!set_explicit_flag(index)) {
                        return false;
                    }
                } else if (!strcmp(a, "--single-dict") || !strcmp(a, "-s")) {
                    if (!set_explicit_flag(2)) {
                        return false;
                    }

                    single_dict = true;
                } else {
                    if (!set_explicit_flag(3)) {
                        return false;
                    }

                    input = a;
                }

                break;
        }

        ++argv;
    }

    return !input.empty();
}

bool Options::set_explicit_flag(int index)
{
    if (explicit_flag[index]) {
        return false;
    }

    explicit_flag[index] = true;
    return true;
}

std::string normalize_existing_path(const std::string &raw)
{
    char buf[PATH_MAX];
    char *res = realpath(raw.c_str(), buf);
    if (!res) {
        std::string msg("cannot normalize ");
        msg += raw;
        throw std::runtime_error(msg);
    }

    return std::string(res);
}

void test_independent(const std::string &seen, size_t n, const std::set<std::string> &dictionary, const Dawg &dawg, CsvWriter &writer)
{
    std::set<std::string> external;
    for (const std::string &correct: dictionary) {
        if (levenshtein_distance(reinterpret_cast<const unsigned char *>(seen.c_str()), reinterpret_cast<const unsigned char *>(correct.c_str())) <= n) {
            external.insert(correct);
        }
    }

    std::vector<std::string> row;
    row.push_back(seen);

    std::set<std::string> internal;
    InputIterator it(seen, n, dawg);
    InputIterator end;
    while (it != end) {
        std::string found = *it;
        row.push_back(found);
        internal.insert(found);

        ++it;
    }

    writer.write_row(row);

    if (external != internal) {
        std::string msg("results for ");
        msg += seen;
        msg += " differ";
        throw std::runtime_error(msg);
    }
}

void test_repeat(const std::string &seen, size_t n, const Dawg &dawg, const CsvReader::Row &row)
{
    bool first = true;
    InputIterator it;
    InputIterator end;
    for (const auto cell: row) {
        std::string value;
        cell.read_value(value);

        if (first) {
            first = false;
            if (value != seen) {
                throw std::runtime_error("result row start mismatch");
            }
        } else {
            if (it == end) {
                it = InputIterator(seen, n, dawg);
            }

            if (value != *it) {
                throw std::runtime_error("result row mismatch");
            }

            ++it;
        }
    }

    if (first) {
        throw std::runtime_error("empty row");
    }

    if (it != end) {
        throw std::runtime_error("result row end mismatch");
    }
}

int main(int argc, char *argv[])
{
    const char *progname = *argv;

    try {
        Options options;
        if (!options.parse(argv)) {
            std::cerr << "usage: " << progname << " [--tolerance TOLERANCE] [--result RESULT] [--single-dict] input" << std::endl;
            return EXIT_FAILURE;
        }

        size_t n = options.tolerance;
        path input_path(normalize_existing_path(options.input));
        path result_path(options.result);
        bool single_mode = options.single_dict;

        std::set<std::string> dictionary = make_test_dict(input_path);
        std::set<std::string> dd = dictionary;
        // actually only needed in single mode
        Dawg dawg = make_dawg(dd);

        if (!std::filesystem::exists(result_path)) {
            std::ofstream stream(result_path);
            CsvWriter writer(stream);

            std::vector<std::string> meta;
            meta.push_back(input_path.native());
            meta.push_back(std::to_string(n));
            meta.push_back(std::to_string(single_mode ? 1 : 0));
            writer.write_row(meta);

            std::string last_word;
            bool first = true;
            for (const std::string &tword: dictionary) {
                std::cerr << tword << "..." << std::endl;

                if (!single_mode) {
                    dd.erase(tword);

                    if (first) {
                        first = false;
                    } else {
                        dd.insert(last_word);
                    }

                    dawg = make_dawg(dd);
                    last_word = tword;
                }

                test_independent(tword, n, dd, dawg, writer);
            }
        } else {
            CsvReader reader;
            reader.mmap(result_path.native());

            const auto header = reader.header();
            size_t hdr_col = 0;
            for (const auto cell: header) {
                std::string value;
                cell.read_value(value);
                switch (hdr_col) {
                    case 0:
                        if (path(value) != input_path) {
                            throw std::runtime_error("input changed");
                        }

                        break;

                    case 1:
                        if (value != std::to_string(n)) {
                            throw std::runtime_error("tolerance changed");
                        }

                        break;

                    case 2:
                        if (value != (single_mode ? "1" : "0")) {
                            throw std::runtime_error("single mode changed");
                        }

                        break;
                }

                ++hdr_col;
            }

            if (hdr_col != 3) {
                throw std::runtime_error("three-column header expected");
            }

            std::string last_word;
            bool first = true;
            CsvReader::RowIterator ln = reader.begin();
            for (const std::string &tword: dictionary) {
                std::cerr << tword << "..." << std::endl;

                if (!single_mode) {
                    dd.erase(tword);

                    if (first) {
                        first = false;
                    } else {
                        dd.insert(last_word);
                    }

                    dawg = make_dawg(dd);
                    last_word = tword;
                }

                test_repeat(tword, n, dawg, *ln);
                ++ln;
            }

            while (ln != reader.end()) {
                for (const auto cell: *ln) {
                    throw std::runtime_error("old result has too many lines");
                }

                ++ln;
            }
        }

        return EXIT_SUCCESS;
    } catch (std::exception &x) {
        std::cerr << progname << ": " << x.what() << std::endl;
        return EXIT_FAILURE;
    }
}
