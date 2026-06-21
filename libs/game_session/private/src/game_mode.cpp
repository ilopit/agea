#include <game_session/game_mode.h>

namespace kryga::game
{

namespace
{
// Meyers singleton: registration happens at static-init across translation units in
// unspecified order, but it only writes here; the slot is read much later (session
// on_init), so init order is irrelevant.
std::function<std::unique_ptr<game_mode>()>&
factory_slot()
{
    static std::function<std::unique_ptr<game_mode>()> f;
    return f;
}
}  // namespace

void
register_game_mode(std::function<std::unique_ptr<game_mode>()> factory)
{
    factory_slot() = std::move(factory);
}

std::unique_ptr<game_mode>
create_registered_game_mode()
{
    auto& f = factory_slot();
    return f ? f() : std::make_unique<game_mode>();
}

}  // namespace kryga::game
