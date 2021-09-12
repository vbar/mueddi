#include "ingest.hh"

#include <fstream>
#include <regex>
#include <stdexcept>

void check_ascii(const std::string &line)
{
    for (std::string::const_iterator it = line.begin(); it != line.end(); ++it) {
        unsigned char c = static_cast<unsigned char>(*it);
        if (!c || (c > 127))
        {
            // mueddi++ library does support UTF-8, but the
            // levenshtein_distance testing function doesn't
            std::string msg("not ASCII: ");
            msg += line;
            throw std::runtime_error(msg);
        }
    }
}

std::set<std::string> make_test_dict(const std::filesystem::path &input_path)
{
    std::set<std::string> dictionary;
    std::regex unword("\\W");
    std::sregex_token_iterator end;
    std::ifstream infile(input_path);
    std::string line;

    while (std::getline(infile, line))
    {
        check_ascii(line);

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
