#include "packages/nevermatch/package.nevermatch.h"

#include "packages/nevermatch/nevermatch_mode.h"

#include <core/package_manager.h>

#include <game_session/game_mode.h>

#include <memory>

namespace kryga::nevermatch
{

package::package()
    : core::package(AID("nevermatch"))
{
}

}  // namespace kryga::nevermatch

// Register this game's mode with the engine session at static-init. Placement matters:
// this TU is retained by the linker because package::package() above is ODR-used by the
// generated load_static_package<package>() schedule (a static initializer in a static
// lib is dropped unless its object file is referenced). The make_unique<nevermatch_mode>
// in turn pulls in nevermatch_mode's TU. Static init runs before main(), so the factory
// is set well before game_session::on_init() reads it. The whole package lib is kept by
// the exe's /WHOLEARCHIVE (see engine/CMakeLists.txt) — without it /OPT:REF strips it.
namespace
{
const int s_nevermatch_mode_registrar = (::kryga::game::register_game_mode(
                                             [] {
                                                 return std::make_unique<
                                                     ::kryga::nevermatch::nevermatch_mode>();
                                             }),
                                         0);
}  // namespace
