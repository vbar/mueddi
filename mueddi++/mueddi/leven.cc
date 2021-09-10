#include "leven.hh"
#include "decoder.hh"
#include "struct_hash.hh"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

namespace mueddi
{

uint32_t CharVec::power_mask[MAX_LEN];
CharVec::Initializer char_vec_init;

Facade::TCacheMap Facade::cache;

bool RelPos::subsumes(const RelPos &other) const
{
    short r = other.edit - edit;
    return (r <= 0) ? false : abs(other.offset - offset) <= r;
}

CharVec::Initializer::Initializer()
{
    uint32_t pwr = 1;
    uint32_t msk = 1;
    for (size_t i = 0; i < MAX_LEN; ++i)
    {
        CharVec::power_mask[i] = msk;
        pwr *= 2;
        msk += pwr;
    }
}

size_t CharVec::get_index_of_set_bit() const
{
    assert(bits);

    size_t index = 1;
    uint32_t b = bits;
    while (!(b & 1)) {
        ++index;
        b /= 2;
    }

    return index;
}

void CharVec::dump(std::ostream &os) const
{
    os << size << ": 0x" << std::hex << bits;
}

size_t ReducedUnion::hash() const
{
    if (!payload->cached_hash) {
        payload->cached_hash = hash_list(payload->pos_list);
    }

    return payload->cached_hash;
}

short ReducedUnion::get_raise_level() const
{
    TPositions::const_iterator it = std::min_element(
        begin(), end(), [](const RelPos &a, const RelPos &b) {
            return a.offset < b.offset; });

    return (it == end()) ? 0 : it->offset;
}

void ReducedUnion::add(const RelPos &rel_pos)
{
    TPositions::iterator nit = std::lower_bound(
        payload->pos_list.begin(), payload->pos_list.end(), rel_pos);

    if ((nit != payload->pos_list.end()) && (rel_pos == *nit)) {
        return;
    }

    for (TPositions::iterator it = payload->pos_list.begin(); it != nit; ++it) {
        if (it->subsumes(rel_pos)) {
            return;
        }
    }

    payload->cached_hash = 0;
    TPositions::iterator it = payload->pos_list.insert(nit, rel_pos);

    assert(it != payload->pos_list.end());
    ++it;

    while (it != payload->pos_list.end()) {
        if (rel_pos.subsumes(*it)) {
            it = payload->pos_list.erase(it);
        } else {
            ++it;
        }
    }
}

void ReducedUnion::add_unchecked(const RelPos &rel_pos)
{
    TPositions::iterator nit = std::lower_bound(
        payload->pos_list.begin(), payload->pos_list.end(), rel_pos);
    assert((nit == payload->pos_list.end()) || (rel_pos != *nit));
    payload->pos_list.insert(nit, rel_pos);
}

void ReducedUnion::update(const ReducedUnion &other)
{
    for (const RelPos &rp: other) {
        add(rp);
    }
}

ReducedUnion ReducedUnion::subtract(short di) const
{
    ReducedUnion red_un = ReducedUnion();
    for (RelPos rp: payload->pos_list) {
        red_un.payload->pos_list.push_back(rp.subtract(di));
    }

    return red_un;
}

void ReducedUnion::dump(std::ostream &os) const
{
    Payload *p = payload.get();
    if (p) {
        os << "[";
        std::string delim(" ");
        for (TPositions::const_iterator i = p->pos_list.begin(); i != p->pos_list.end(); ++i) {
            os << delim << *i;
            delim = ", ";
        }

        os << " ]";
    } else {
        os << "<null>";
    }
}

ReducedUnion Elementary::elem_delta(size_t i, size_t w, const RelPos &rel_pos, const CharVec &char_vec) const
{
#if 0
    std::cerr << "enter Elementary.elem_delta(" << i << ", " << w << ", " << rel_pos << ", " <<  char_vec << ')' << std::endl;
#endif

    size_t rl = get_rel_pos_len(i + rel_pos.offset, w, rel_pos.edit);
    CharVec loc_char_vec = ((rl < char_vec.size) || (rel_pos.offset > 0))
        ? char_vec.subrange(rl, 1 + rel_pos.offset)
        : char_vec;
    return (rel_pos.edit < n)
        ? deltaI(rel_pos, loc_char_vec)
        : deltaII(rel_pos, loc_char_vec);
}

ReducedUnion Elementary::deltaI(const RelPos &rel_pos, const CharVec &char_vec) const
{
    ReducedUnion result = ReducedUnion();
    if (char_vec.is_empty()) {
        result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1));
        return result;
    }

    if (char_vec.size == 1) {
        if (char_vec.has_first_bit_set()) {
            result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit));
        } else {
            result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1));
            result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit + 1));
        }
    } else {
        if (char_vec.has_first_bit_set()) {
            result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit));
        } else {
            result.add_unchecked(RelPos(rel_pos.offset, rel_pos.edit + 1));
            result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit + 1));
            if (char_vec.bits) {
                size_t j = char_vec.get_index_of_set_bit();
                result.add_unchecked(RelPos(rel_pos.offset + j, rel_pos.edit + j - 1));
            }
        }
    }

    return result;
}

ReducedUnion Elementary::deltaII(const RelPos &rel_pos, const CharVec &char_vec) const
{
    ReducedUnion result = ReducedUnion();
    if (char_vec.has_first_bit_set()) {
        result.add_unchecked(RelPos(rel_pos.offset + 1, rel_pos.edit));
    }

    return result;
}

size_t Elementary::get_rel_pos_len(size_t i, size_t w, short e) const
{
    assert(w >= i);
    return std::min(n - e + 1, w - i);
}

LazyTable::LazyTable()
{
    ReducedUnion zero;
    zero.add_unchecked(RelPos(0, 0));
    state2transition.emplace(zero, TTransitionMap());
}

size_t LazyTable::get_rel_state_len(size_t i, size_t w) const
{
    assert(w >= i);
    return std::min(2 * n + 1, w - i);
}

ReducedUnion LazyTable::delta(const LevenState &pinned_state, size_t w, const CharVec &char_vec)
{
    size_t i = pinned_state.base;
    TTransitionMap &transition = state2transition[pinned_state.reduced_union];
    auto p = transition.emplace(char_vec, ReducedUnion());
    TTransitionMap::iterator redit = p.first;
    if (p.second) {
        for (const RelPos &rp: pinned_state.reduced_union) {
            redit->second.update(elem_delta(i, w, rp, char_vec));
        }
    }

    return redit->second;
}

CharVec LazyTable::make_char_vec(const std::string &sub_word, uint32_t letter)
{
#if 0
    char buf[5];
    utf8_encode(buf, letter);
    std::cerr << "enter make_char_vec(\"" << sub_word << "\", '" << buf << "')" << std::endl;
#endif

    const unsigned char *u = reinterpret_cast<const unsigned char *>(sub_word.c_str());
    uint32_t bits = 0;
    size_t char_len = 0;
    uint32_t pwr = 1;
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0xdeadbeef;
    while (*u) {
        while (decode(&state, &codepoint, *u)) {
            ++u;
        }

        ++char_len;

        assert(state == UTF8_ACCEPT);

        if (codepoint == letter) {
            bits |= pwr;
        }

        pwr *= 2;
        ++u;
    }

    return CharVec(bits, char_len);
}

Facade::Facade(const std::string &word, size_t n):
    payload(std::make_shared<Payload>(word, get_code_point_count(reinterpret_cast<const unsigned char *>(word.c_str()))))
{
    if (n > 15) {
        throw std::runtime_error("number of corrections too big for this package");
    }

    LazyTable &lt = cache[n];
    if (!lt.n) {
        lt.n = n;
    }

    payload->lazy_table = &lt;
}

bool Facade::is_final(const LevenStateRef &state) const
{
    LevenState *ls = state.get();
    assert(ls);

    size_t w = payload->w;
    size_t n = payload->lazy_table->n;
    for (RelPos rp: ls->reduced_union) {
        size_t i = ls->base + rp.offset;
        if ((w + rp.edit) <= (n + i)) {
            return true;
        }
    }

    return false;
}

LevenStateRef Facade::delta(const LevenStateRef &cur_state, uint32_t letter)
{
#if 0
    char buf[5];
    utf8_encode(buf, letter);
    std::cerr << "enter Facade.delta(" << cur_state << ", " << buf << ')' << std::endl;
#endif

    assert(cur_state.get() && !cur_state->reduced_union.get_raise_level());

    LazyTable *lazy_table = payload->lazy_table;
    size_t i = cur_state->base;
    size_t rl = lazy_table->get_rel_state_len(i, payload->w);

    const unsigned char *start = nullptr, *u = reinterpret_cast<const unsigned char *>(payload->word.c_str());
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0xdeadbeef;
    size_t cnt = 0;
    while (*u && (cnt < i + rl)) {
        if (cnt == i) {
            start = u;
        }

        while (decode(&state, &codepoint, *u)) {
            ++u;
        }

        assert(state == UTF8_ACCEPT); // checked in constructor

        ++u;
        ++cnt;
    }

    std::string rel_word;
    if (start) {
        rel_word.assign(reinterpret_cast<const char *>(start), reinterpret_cast<const char *>(u));
    }

    CharVec char_vec = lazy_table->make_char_vec(rel_word, letter);
    ReducedUnion image = lazy_table->delta(*cur_state, payload->w, char_vec);
    if (image.is_empty()) {
        return LevenStateRef();
    }

    short di = image.get_raise_level();
    ReducedUnion cc = di ? image.subtract(di) : image;
    return std::make_shared<LevenState>(i + di, cc);
}

LevenStateRef Facade::initial_state()
{
    RelPos zero_pos = RelPos(0, 0);
    ReducedUnion zero_union = ReducedUnion();
    zero_union.add_unchecked(zero_pos);
    return std::make_shared<LevenState>(0, zero_union);
}

}
