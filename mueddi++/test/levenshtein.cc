#include "levenshtein.hh"

#include <algorithm>
#include <memory>
#include <assert.h>
#include <string.h>

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

size_t levenshtein_distance(const char *s, size_t m, const char *t, size_t n)
{
    Matrix d(m + 1, n + 1);

    for (size_t i = 0; i <= m; i++) {
        d.at(i, 0) = i;
    }

    for(size_t j = 1; j <= n; j++) {
        d.at(0, j) = j;
    }

    for (size_t i = 1; i <= m; i++)
    {
        for (size_t j = 1; j <= n; j++)
        {
            if (s[i - 1] == t[j - 1])
            {
                d.at(i, j) = d.at(i - 1, j - 1);
            }
            else
            {
                d.at(i, j) = 1 + min3(d.at(i - 1, j), d.at(i, j - 1), d.at(i - 1, j - 1));
            }
        }
    }

    return d.at(m, n);
}
