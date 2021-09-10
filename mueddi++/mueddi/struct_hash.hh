#ifndef mueddi_struct_hash_hh
#define mueddi_struct_hash_hh

#include <stddef.h>

namespace mueddi
{

template<typename C>
size_t hash_list(const C &container)
{
    const size_t MOD_ADLER = 65521;
    size_t a = 1;
    size_t b = 0;

    for (const auto &item: container) {
        a = (a + item.hash()) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

}

#endif
