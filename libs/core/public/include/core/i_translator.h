#pragma once

namespace kryga
{

// ---------------------------------------------------------------------------
// i_translator
//
// Shared shape of the model-thread translators (render / physics / audio): each
// bridges the model to a subsystem. The hooks below give them ONE vocabulary for
// the lifecycle + per-frame maintenance every translator has some form of.
//
// They are deliberately NOT collapsed into a single engine-side loop: each runs at
// a subsystem-specific point (connect after that subsystem's init, disconnect before
// its teardown, the per-frame step ordered against the frame's build), and the call
// sites stay placed for that ordering. The interface unifies the NAMES, not the
// schedule. All hooks default to no-op so a translator implements only what applies
// (audio has no storages to connect; a translator with no per-frame work skips it).
//
// Producer translators (audio/physics) reach this through translator_base<TMessage>;
// render_translator derives directly (it is a different producer shape). Threading:
// every hook runs on the model/main thread.
// ---------------------------------------------------------------------------
class i_translator
{
public:
    virtual ~i_translator() = default;

    // [init, model thread] Claim subsystem-side resources (e.g. storage lane claims),
    // once, after the owning subsystem exists.
    virtual void
    connect()
    {
    }

    // [shutdown, model thread] Release what connect() claimed, before the owning
    // subsystem (and its storages) is destroyed.
    virtual void
    disconnect()
    {
    }

    // [model thread] Once-per-frame maintenance: mature deferred frees / drain the
    // result ring / reap orphaned voices, depending on the translator.
    virtual void
    on_frame()
    {
    }
};

}  // namespace kryga
