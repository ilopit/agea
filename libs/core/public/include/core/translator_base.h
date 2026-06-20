#pragma once

#include <core/i_translator.h>

#include <utils/spsc_queue.h>

namespace kryga
{

// ---------------------------------------------------------------------------
// translator_base<TMessage>
//
// Common base for a model-thread PRODUCER translator: it copies POD value messages
// onto a subsystem's lock-free SPSC ring. Parameterized on the message type so the
// emit plumbing is written ONCE -- audio_translator (audio_message) and
// physics_translator (physics_message) derive from it.
//
// render_translator deliberately does NOT use this base: it is a different producer
// shape (double-buffered pointer commands in a per-frame arena -- see command_queue
// in subsystem_queues.h), not a value ring, so a value-message base would not fit.
//
// The target ring is injected at construction. subsystem_queues is created before any
// translator (see kryga_engine init order), so the ring outlives every emit(); teardown
// never dereferences the held reference. Threading: emit() runs on the model/main
// thread, which is the sole producer of its ring.
// ---------------------------------------------------------------------------
template <typename TMessage>
class translator_base : public i_translator
{
public:
    explicit translator_base(utils::spsc_queue<TMessage>& out)
        : m_out(out)
    {
    }

protected:
    // Copy the POD intent into the ring (value SPSC). push() spin-blocks if the ring
    // is full, but every channel is sized well above the realistic per-frame intent
    // count. The sole producer is the calling (model) thread.
    void
    emit(const TMessage& msg)
    {
        m_out.push(TMessage(msg));
    }

private:
    utils::spsc_queue<TMessage>& m_out;
};

}  // namespace kryga
