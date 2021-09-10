#include "dawg.hh"

#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <stdlib.h>

using namespace mueddi;

int main(int argc, char *argv[])
{
    try {
        TWords words;
        bool first = true;
        while (*argv) {
            if (first) {
                first = false;
            } else {
                words.push_back(std::string(*argv));
            }

            ++argv;
        }

        if (words.empty()) {
            throw std::runtime_error("no words");
        }

        std::set<std::string> word_set(words.begin(), words.end());
        if (word_set.size() < words.size()) {
            throw std::runtime_error("duplicate words");
        }

        Dawg dawg = make_dawg(words);

        std::string neg("~");
        for (auto &w: words) {
            if (!dawg.accepts(w)) {
                std::string msg = "does not accept ";
                msg += w;
                throw std::runtime_error(msg);
            }

            neg += w;
        }

        if (dawg.accepts(neg)) {
            std::string msg = "accepts ";
            msg += neg;
            throw std::runtime_error(msg);
        }

        return EXIT_SUCCESS;
    } catch (std::exception &x) {
        std::cerr << "mueddi failed: " << x.what() << std::endl;
        return EXIT_FAILURE;
    }
}
