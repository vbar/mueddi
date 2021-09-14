#include "levenshtein.hh"
#include "decoder.hh"

#include <algorithm>
#include <memory>
#include <assert.h>
#include <string.h>

using mueddi::decode;
using mueddi::get_code_point_count;

class Matrix
{
public:
    Matrix(size_t m, size_t n);
    ~Matrix();
    Matrix(const Matrix&) = delete;
    Matrix &operator=(const Matrix&) = delete;

    size_t at(size_t i, size_t j) const;
    size_t &at(size_t i, size_t j);

private:
    const size_t stride;
    const size_t sz;
    size_t *buffer;
};

Matrix::Matrix(size_t m, size_t n):
    stride(n),
    sz(m * n)
{
    assert(m);
    assert(n);
    buffer = new size_t[sz];
}

Matrix::~Matrix()
{
    delete [] buffer;
}

size_t Matrix::at(size_t i, size_t j) const
{
    size_t idx = stride * i + j;
    assert(idx < sz);
    return buffer[idx];
}

size_t &Matrix::at(size_t i, size_t j)
{
    size_t idx = stride * i + j;
    assert(idx < sz);
    return buffer[idx];
}

inline size_t min3(size_t a, size_t b, size_t c) {
    return std::min(a, std::min(b, c));
}

size_t levenshtein_distance(const unsigned char *s, const unsigned char *t)
{
    size_t m = get_code_point_count(s);
    size_t n = get_code_point_count(t);
    if (m < n) {
        std::swap(s, t);
        std::swap(m, n);
    }

    if (!n) {
        return m;
    }

    Matrix d(m + 1, n + 1);

    for (size_t i = 0; i <= m; i++) {
        d.at(i, 0) = i;
    }

    for(size_t j = 1; j <= n; j++) {
        d.at(0, j) = j;
    }

    std::unique_ptr<uint32_t[]> cache(new uint32_t[n]);
    uint32_t *w = cache.get();

    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0xdeadbeef;
    size_t ci = 0;
    while (*t) {
        while (decode(&state, &codepoint, *t)) {
            ++t;
        }

        assert(state == UTF8_ACCEPT);

        w[ci++] = codepoint;
        ++t;
    }

    assert(ci == n);

    size_t i = 1;
    while (*s) {
        while (decode(&state, &codepoint, *s)) {
            ++s;
        }

        assert(state == UTF8_ACCEPT);

        for (size_t j = 1; j <= n; j++)
        {
            if (codepoint == w[j - 1])
            {
                d.at(i, j) = d.at(i - 1, j - 1);
            }
            else
            {
                d.at(i, j) = 1 + min3(d.at(i - 1, j), d.at(i, j - 1), d.at(i - 1, j - 1));
            }
        }

        ++s;
        ++i;
    }

    assert(i == m + 1);

    return d.at(m, n);
}
