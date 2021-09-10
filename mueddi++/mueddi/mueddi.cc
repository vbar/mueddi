#include "mueddi.hh"
#include "dawg.hh"
#include "encoder.hh"
#include "leven.hh"

#include <queue>
#include <assert.h>

namespace mueddi
{

class QueueItem
{
public:
    std::string candidate;
    DawgStateRef dawg_state;
    LevenStateRef leven_state;

    QueueItem(const std::string &v, const DawgStateRef &q, const LevenStateRef &m);
    QueueItem(const QueueItem &other) = default;
    QueueItem &operator=(const QueueItem &other) = default;
};

using TQueue = std::queue<QueueItem>;

class IteratorPayload
{
public:
    Dawg dawg;
    Facade facade;
    TQueue queue;
    std::string current;
    bool valid;

    IteratorPayload(const std::string &seen, size_t n, const Dawg &dawg);
    ~IteratorPayload() = default;
    IteratorPayload(const IteratorPayload &) = delete;
    IteratorPayload &operator=(const IteratorPayload &) = delete;
};

inline QueueItem::QueueItem(const std::string &v, const DawgStateRef &q, const LevenStateRef &m):
    candidate(v),
    dawg_state(q),
    leven_state(m)
{
}

InputIterator::InputIterator(const std::string &seen, size_t n, const Dawg &dawg):
    payload(std::make_shared<IteratorPayload>(seen, n, dawg))
{
    advance();
}

InputIterator::InputIterator()
{
}

InputIterator::~InputIterator()
{
}

InputIterator::InputIterator(const InputIterator &other):
    payload(other.payload)
{
}

InputIterator &InputIterator::operator=(const InputIterator &other)
{
    payload = other.payload;
    return *this;
}

bool InputIterator::at_end() const
{
    IteratorPayload *p = payload.get();
    return !p || (!p->valid && p->queue.empty());
}

bool InputIterator::operator==(const InputIterator &other) const
{
    if (at_end()) {
        return other.at_end();
    }

    if (other.at_end()) {
        return false;
    }

    return payload.get() == other.payload.get();
}

std::string InputIterator::operator*()
{
    assert(!at_end());
    return payload->current;
}

void InputIterator::advance()
{
    char buf[5];

    IteratorPayload *p = payload.get();
    assert(p);

    p->valid = false;
    while (!p->valid && !p->queue.empty()) {
        QueueItem item = p->queue.front();
        p->queue.pop();
        if (item.dawg_state->is_final() && p->facade.is_final(item.leven_state)) {
            p->current = item.candidate;
            p->valid = true;
        }

        for (TChildren::const_iterator it = item.dawg_state->begin(); it != item.dawg_state->end(); ++it) {
            uint32_t x = it->first;
            LevenStateRef mp = p->facade.delta(item.leven_state, x);
            if (mp.get()) {
                std::string v1(item.candidate);
                size_t l = utf8_encode(buf, x);
                assert(l);
                v1.append(buf, l);

                DawgStateRef q1 = item.dawg_state->get_child(x);
                assert(q1.get());

                p->queue.emplace(v1, q1, mp);
            }
        }
    }
}

IteratorPayload::IteratorPayload(const std::string &seen, size_t n, const Dawg &dawg):
    dawg(dawg),
    facade(seen, n),
    valid(false)
{
    queue.emplace(std::string(), this->dawg.get_root(), Facade::initial_state());
}

}
