#ifndef mueddi_dawg_hh
#define mueddi_dawg_hh

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mueddi
{

class DawgState;

class Builder;

using DawgStateRef = std::shared_ptr<DawgState>;

using TWords = std::vector<std::string>;

using TChildren = std::map<uint32_t, DawgStateRef>;

class DawgState
{
public:
    DawgState(bool finl);
    ~DawgState() = default;
    DawgState(const DawgState &) = delete;
    DawgState &operator=(const DawgState &) = delete;

    bool operator==(const DawgState &other) const = default;
    bool operator<(const DawgState &other) const = default;

    bool is_final() const;

    bool has_children() const;

    TChildren::const_iterator begin() const;
    TChildren::const_iterator end() const;

    DawgStateRef get_child(uint32_t letter);

    DawgStateRef last_child();

    void set_last_child(const DawgStateRef &child);

    void add_child(uint32_t letter, const DawgStateRef &child);

    void dump(std::ostream &os) const;

private:
    const bool finl;
    TChildren children;
};

inline std::ostream &operator<<(std::ostream &os, const DawgStateRef &ref)
{
    const DawgState *state = ref.get();
    if (state) {
        state->dump(os);
    } else {
        os << "<null>";
    }

    return os;
}

class Dawg
{
public:
    Dawg(bool root_final);
    Dawg(const Dawg &other);
    ~Dawg();
    Dawg &operator=(const Dawg &other);
    Dawg(Dawg &&other) = default;
    Dawg& operator=(Dawg &&other) = default;

    bool accepts(const std::string &w);

    DawgStateRef get_root() const;

private:
    friend class Builder;

    using TPrefixState = std::pair<std::string, DawgStateRef>;

    DawgStateRef root;

    TPrefixState track_prefix(const std::string &word);
};

inline std::ostream &operator<<(std::ostream &os, const Dawg &dawg)
{
    os << "Dawg: " << dawg.get_root();
    return os;
}

Dawg make_dawg_impl(TWords &words); // param modified in-place

template<typename I>
Dawg make_dawg(I b, I e)
{
    TWords w(b, e);
    return make_dawg_impl(w);
}

template<typename R>
Dawg make_dawg(const R &c)
{
    return make_dawg(std::begin(c), std::end(c));
}

inline DawgState::DawgState(bool finl):
    finl(finl)
{
}

inline bool DawgState::is_final() const
{
    return finl;
}

inline bool DawgState::has_children() const
{
    return !children.empty();
}

inline TChildren::const_iterator DawgState::begin() const
{
    return children.begin();
}

inline TChildren::const_iterator DawgState::end() const
{
    return children.end();
}

inline DawgStateRef Dawg::get_root() const
{
    return root;
}

}

#endif
