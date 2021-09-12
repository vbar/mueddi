#include "levenshtein.hh"

#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "usage: " << *argv << " word1 word2" << std::endl;
        return EXIT_FAILURE;
    }

    size_t d = levenshtein_distance(argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
    std::cout << d << std::endl;

    return EXIT_SUCCESS;
}
