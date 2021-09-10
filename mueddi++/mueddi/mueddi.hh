#ifndef mueddi_mueddi_hh
#define mueddi_mueddi_hh

#include <memory>
#include <string>

// for itself, this header could forward-declare, but it doubles as a
// library-wide include for all externally used classes
#include "dawg.hh"

namespace mueddi
{

class IteratorPayload;

class InputIterator
{
public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = void;
    using value_type = std::string;
    using pointer = std::string *;
    using reference = std::string &;

    InputIterator(const std::string &seen, size_t n, const Dawg &dawg);
    InputIterator();
    ~InputIterator();
    InputIterator(const InputIterator &other);
    InputIterator &operator=(const InputIterator &other);

    bool operator==(const InputIterator &other) const;

    std::string operator*();
    InputIterator &operator++();

private:
    bool at_end() const;

    void advance();

    std::shared_ptr<IteratorPayload> payload;
};

inline InputIterator &InputIterator::operator++()
{
    advance();
    return *this;
}

}

#endif
