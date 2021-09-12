#ifndef ingest_hh
#define ingest_hh

#include <filesystem>
#include <set>
#include <string>

std::set<std::string> make_test_dict(const std::filesystem::path &input_path);

#endif
