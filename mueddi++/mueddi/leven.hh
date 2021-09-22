#ifndef mueddi_leven_hh
#define mueddi_leven_hh

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stddef.h>

namespace mueddi
{

static const size_t MAX_LEN = 31;

class RelPos
{
public:
    short offset;
    short edit;

    RelPos(short o, short e);
    ~RelPos() = default;
    RelPos(const RelPos &) = default;
    RelPos &operator=(const RelPos &) = default;

    bool operator==(const RelPos &other) const = default;
    bool operator<(const RelPos &other) const;

    size_t hash() const;

    bool subsumes(const RelPos &other) const;

    RelPos subtract(short di) const;
};

inline std::ostream &operator<<(std::ostream &os, const RelPos &rp)
{
    os << '+' << rp.offset << '#' << rp.edit;
    return os;
}

class CharVec
{
public:
    class Initializer
    {
    public:
        Initializer();
    };

    const uint32_t bits;
    const size_t size;

    CharVec(uint32_t b, size_t s);
    ~CharVec() = default;
    CharVec(const CharVec &other) = default;
    CharVec &operator=(const CharVec &other) = default;

    bool operator==(const CharVec &other) const = default;
    bool operator<(const CharVec &other) const;

    bool is_empty() const;

    CharVec subrange(size_t sz, size_t sh) const;

    bool has_first_bit_set() const;

    // caller must ensure some bit is set
    size_t get_index_of_set_bit() const;

    void dump(std::ostream &os) const;

private:
    friend class Initializer;

    static uint32_t power_mask[MAX_LEN];
};

inline std::ostream &operator<<(std::ostream &os, const CharVec &cv)
{
    cv.dump(os);
    return os;
}

class ReducedUnion
{
public:
    using TPositions = std::vector<RelPos>;

    ReducedUnion();
    ~ReducedUnion() = default;
    ReducedUnion(const ReducedUnion &other) = default;
    ReducedUnion &operator=(const ReducedUnion &other) = default;
    ReducedUnion(ReducedUnion &&other) = default;
    ReducedUnion& operator=(ReducedUnion &&other) = default;

    bool operator==(const ReducedUnion &other) const;

    TPositions::const_iterator begin() const;
    TPositions::const_iterator end() const;

    bool is_empty() const;

    size_t hash() const;

    short get_raise_level() const;

    void add(const RelPos &rel_pos);

    void add_unchecked(const RelPos &rel_pos);

    void update(const ReducedUnion &other);

    ReducedUnion subtract(short di) const;

    void dump(std::ostream &os) const;

private:
    class Payload
    {
    public:
        TPositions pos_list;
        size_t cached_hash;

        Payload();
        ~Payload() = default;
        Payload(const Payload &) = delete;
        Payload &operator=(const Payload &) = delete;
    };

    std::shared_ptr<Payload> payload;
};

}

namespace std
{

template <>
struct hash<mueddi::ReducedUnion>
{
    std::size_t operator()(const mueddi::ReducedUnion &ru) const
    {
        return ru.hash();
    }
};

}

namespace mueddi
{

inline std::ostream &operator<<(std::ostream &os, const ReducedUnion &ru)
{
    ru.dump(os);
    return os;
}

class Elementary
{
public:
    size_t n;

    Elementary();
    ~Elementary() = default;
    Elementary(const Elementary &) = delete;
    Elementary &operator=(const Elementary &) = delete;

    ReducedUnion elem_delta(size_t i, size_t w, const RelPos &rel_pos, const CharVec &char_vec) const;

private:
    ReducedUnion deltaI(const RelPos &rel_pos, const CharVec &char_vec) const;

    ReducedUnion deltaII(const RelPos &rel_pos, const CharVec &char_vec) const;

    size_t get_rel_pos_len(size_t i, size_t w, short e) const;
};

class LevenState
{
public:
    const size_t base;
    const ReducedUnion reduced_union;

    LevenState(size_t b, const ReducedUnion &ru);
    ~LevenState() = default;
    LevenState(const LevenState &other) = default;
    LevenState &operator=(const LevenState &other) = default;
};

using LevenStateRef = std::shared_ptr<LevenState>;

inline std::ostream &operator<<(std::ostream &os, const LevenStateRef &ref)
{
    const LevenState *state = ref.get();
    if (state) {
        os << state->base << ": " << state->reduced_union;
    } else {
        os << "<null>";
    }

    return os;
}

class LazyTable : public Elementary
{
public:
    LazyTable();

    size_t get_rel_state_len(size_t i, size_t w) const;

    ReducedUnion delta(const LevenState &pinned_state, size_t w, const CharVec &char_vec);

    static CharVec make_char_vec(const std::string &sub_word, uint32_t letter);

private:
    using TTransitionMap = std::map<CharVec, ReducedUnion>;
    using TLazyMap = std::unordered_map<ReducedUnion, TTransitionMap>;

    TLazyMap state2transition;
};

class Facade
{
public:
    Facade(const std::string &word, size_t n);
    Facade(const Facade &other) = default;
    Facade &operator=(const Facade &other) = default;

    bool is_final(const LevenStateRef &state) const;

    LevenStateRef delta(const LevenStateRef &cur_state, uint32_t letter);

    static LevenStateRef initial_state();

private:
    using TCacheMap = std::map<size_t, LazyTable>;

    class Payload
    {
    public:
        const std::string word;
        const size_t w;
        LazyTable *lazy_table;

        Payload(const std::string &word, size_t w);
        ~Payload() = default;
        Payload(const Payload &) = delete;
        Payload &operator=(const Payload &) = delete;
    };

    std::shared_ptr<Payload> payload;

    static TCacheMap cache;
};

inline RelPos::RelPos(short o, short e):
    offset(o),
    edit(e)
{
}

inline bool RelPos::operator<(const RelPos &other) const
{
    return (edit < other.edit) || ((edit == other.edit) && (offset < other.offset));
}

inline size_t RelPos::hash() const
{
    return (MAX_LEN + 1) * offset + edit;
}

inline RelPos RelPos::subtract(short di) const
{
    return RelPos(offset - di, edit);
}

inline CharVec::CharVec(uint32_t b, size_t s):
    bits(b),
    size(s)
{
}

inline bool CharVec::operator<(const CharVec &other) const
{
    return (size < other.size) || ((size == other.size) && (bits < other.bits));
}

inline bool CharVec::is_empty() const
{
    return !size;
}

inline CharVec CharVec::subrange(size_t sz, size_t sh) const
{
    return sz ? CharVec((bits >> (sh - 1)) & power_mask[sz - 1], sz) : CharVec(0, 0);
}

inline bool CharVec::has_first_bit_set() const
{
    return bits & 1;
}

inline ReducedUnion::ReducedUnion():
    payload(std::make_shared<Payload>())
{
}

inline bool ReducedUnion::operator==(const ReducedUnion &other) const
{
    return payload->pos_list == other.payload->pos_list;
}

inline ReducedUnion::TPositions::const_iterator ReducedUnion::begin() const
{
    return payload->pos_list.begin();
}

inline ReducedUnion::TPositions::const_iterator ReducedUnion::end() const
{
    return payload->pos_list.end();
}

inline bool ReducedUnion::is_empty() const
{
    return payload->pos_list.empty();
}

inline ReducedUnion::Payload::Payload():
    cached_hash(0)
{
}

inline Elementary::Elementary():
    n(0)
{
}

inline LevenState::LevenState(size_t b, const ReducedUnion &ru):
    base(b),
    reduced_union(ru)
{
}

inline Facade::Payload::Payload(const std::string &word, size_t w):
    word(word),
    w(w),
    lazy_table(nullptr)
{
}

}

#endif
