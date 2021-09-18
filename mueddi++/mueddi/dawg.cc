#include "dawg.hh"
#include "decoder.hh"
#include "encoder.hh"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <assert.h>
#include <string.h>

namespace mueddi
{

class Builder
{
public:
    Builder(bool root_final);
    Builder(const Builder &) = delete;
    Builder &operator=(const Builder &) = delete;

    void build(const TWords &words);

    Dawg dawg;

private:
    using TRegister = std::set<DawgStateRef>;

    void replace_or_register(DawgStateRef state);

    void add_suffix(const DawgStateRef &state, const std::string &suffix);

    TRegister registr;
};

DawgStateRef DawgState::get_child(uint32_t letter)
{
    auto it = children.find(letter);
    if (it == children.end()) {
        return DawgStateRef();
    } else {
        return it->second;
    }
}

DawgStateRef DawgState::last_child()
{
    auto it = children.rbegin();
    if (it == children.rend()) {
        return DawgStateRef();
    } else {
        return it->second;
    }
}

void DawgState::set_last_child(const DawgStateRef &child)
{
    auto it = children.rbegin();
    assert(it != children.rend());
    it->second = child;
}

void DawgState::add_child(uint32_t letter, const DawgStateRef &child)
{
    auto p = children.insert(TChildren::value_type(letter, child));
    assert(p.second);
}

void DawgState::dump(std::ostream &os) const
{
    char buf[5];

    os << ( finl ? 't' : 'f' ) << " {";
    std::string delim(" ");
    for (TChildren::const_iterator it = children.begin(); it != children.end(); ++it) {
        os << delim;
        utf8_encode(buf, it->first);
        os << '\'' << buf << "\': " << it->second;
        delim = ", ";
    }

    os << " }";
}

Dawg::Dawg(bool root_final):
    root(std::make_shared<DawgState>(root_final))
{
}

Dawg::Dawg(const Dawg &other):
    root(other.root)
{
}

Dawg::~Dawg()
{
}

Dawg &Dawg::operator=(const Dawg &other)
{
    root = other.root;
    return *this;
}

bool Dawg::accepts(const std::string &w)
{
    const unsigned char *u = reinterpret_cast<const unsigned char *>(w.c_str());

    DawgStateRef node = root;
    assert(node.get());

    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0xdeadbeef;
    while (true) {
        while (decode(&state, &codepoint, *u)) {
            ++u;
        }

        if (state != UTF8_ACCEPT) {
            throw std::runtime_error("invalid UTF-8");
        }

        if (!*u) {
            assert(!codepoint);
            return node->is_final();
        }

        node = node->get_child(codepoint);
        if (!node) {
            return false;
        }

        ++u;
    }
}

Dawg::TPrefixState Dawg::track_prefix(const std::string &word)
{
    const unsigned char *start, *u = reinterpret_cast<const unsigned char *>(word.c_str());
    std::string prefix;
    DawgStateRef next_state = root;
    DawgStateRef prev_state = next_state; // for empty word

    uint32_t state = UTF8_ACCEPT;
    uint32_t ch = 0xdeadbeef;
    while (true) {
        start = u;
        while (decode(&state, &ch, *u)) {
            ++u;
        }

        if (state != UTF8_ACCEPT) {
            throw std::runtime_error("word has invalid UTF-8");
        }

        if (!*u) {
            break;
        }

        ++u;

        prev_state = next_state;
        next_state = prev_state->get_child(ch);
        if (!next_state) {
            break;
        } else {
            prefix.append(reinterpret_cast<const char *>(start), reinterpret_cast<const char *>(u));
        }
    }

    return TPrefixState(prefix, prev_state);
}

inline Builder::Builder(bool root_final):
    dawg(root_final)
{
}

void Builder::build(const TWords &words)
{
    for (auto &word: words) {
        Dawg::TPrefixState prefix_state = dawg.track_prefix(word);
        assert(prefix_state.second.get());
        std::string current_suffix(word.begin() + prefix_state.first.size(), word.end());
        if (prefix_state.second->has_children()) {
            replace_or_register(prefix_state.second);
        }

        add_suffix(prefix_state.second, current_suffix);
    }

    replace_or_register(dawg.root);
}

void Builder::replace_or_register(DawgStateRef state)
{
    assert(state.get());
    DawgStateRef child = state->last_child();
    if (!!child && child->has_children()) {
        replace_or_register(child);
    }

    auto it = registr.find(child);
    if (it != registr.end()) {
        state->set_last_child(*it);
    } else {
        registr.insert(child);
    }
}

void Builder::add_suffix(const DawgStateRef &state, const std::string &suffix)
{
    assert(state.get());
    const unsigned char *u = reinterpret_cast<const unsigned char *>(suffix.c_str());

    DawgStateRef prev_state = state;
    uint32_t char_state = UTF8_ACCEPT;
    uint32_t ch = 0xdeadbeef;
    while (*u) {
        while (decode(&char_state, &ch, *u)) {
            ++u;
        }

        if (char_state != UTF8_ACCEPT) {
            throw std::runtime_error("suffix has invalid UTF-8");
        }

        ++u;

        DawgStateRef next_state = std::make_shared<DawgState>(!*u);
        prev_state->add_child(ch, next_state);
        registr.insert(next_state);
        prev_state = next_state;
    }
}

Dawg make_dawg_impl(TWords &words)
{
    // UTF-8 can be compared per-character, but for the ordering to be
    // by Unicode codepoints (which this program requires), the
    // character must be unsigned
    std::sort(words.begin(), words.end(),
              [](const std::string &a, const std::string &b) {
                  return strcmp(a.c_str(), b.c_str()) < 0;
              });

    Builder builder(words.empty() || words[0].empty());
    builder.build(words);
    return builder.dawg;
}

}
