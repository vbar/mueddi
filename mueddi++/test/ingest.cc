#include "ingest.hh"

#include <fstream>
#include <regex>
#include <stdexcept>

std::set<std::string> make_test_dict(const std::filesystem::path &input_path)
{
    std::set<std::string> dictionary;
    std::regex unword("[\r\n\t .?!,;:\"'()\\[\\]{}&*#$@_]");
    std::sregex_token_iterator end;
    std::ifstream infile(input_path);
    std::string line;

    while (std::getline(infile, line))
    {
        std::sregex_token_iterator iter(line.begin(), line.end(), unword, -1);
        while (iter != end) {
            std::string word(*iter);
            if (!word.empty()) {
                dictionary.insert(word);
            }

            ++iter;
        }
    }

    return dictionary;
}
