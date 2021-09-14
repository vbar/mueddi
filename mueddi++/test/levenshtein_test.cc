#include "levenshtein.hh"

#include <iomanip>
#include <iostream>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "usage: " << *argv << " word1 word2" << std::endl;
        return EXIT_FAILURE;
    }

    size_t d = levenshtein_distance(
        reinterpret_cast<unsigned char *>(argv[1]),
        reinterpret_cast<unsigned char *>(argv[2]));
    std::cout << d << std::endl;

    return EXIT_SUCCESS;
}
