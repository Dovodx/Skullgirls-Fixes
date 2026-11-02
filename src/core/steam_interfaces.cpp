#include "core/steam_interfaces.h"

#include "core/game_addresses.h"
#include "core/game_image.h"

namespace sgfixes::steam_interfaces {

void* FromContext(uintptr_t preferredContextAddress) {
    using ContextInit = void** (__cdecl*)(void* context);
    auto importSlot = reinterpret_cast<ContextInit*>(
        game_image::Rebase(game_addresses::kSteamContextInitImport));
    void* context = reinterpret_cast<void*>(game_image::Rebase(preferredContextAddress));

    void** interfaceSlot = (*importSlot)(context);
    return interfaceSlot == nullptr ? nullptr : *interfaceSlot;
}

void* VirtualFunction(void* steamInterface, size_t slot) {
    void** table = *static_cast<void***>(steamInterface);
    return table[slot];
}

unsigned ShortTag(uint64_t steamId) {
    return static_cast<unsigned>(steamId & 0xffff);
}

}  // namespace sgfixes::steam_interfaces
