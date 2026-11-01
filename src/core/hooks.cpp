#include "core/hooks.h"

#include <MinHook.h>

#include <cstring>

#include "core/game_image.h"
#include "core/log.h"

namespace sgfixes::hooks {

void Begin() {
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        log::Writef("mod", "MinHook failed to start: %s", MH_StatusToString(status));
    }
}

bool Create(const char* name, uintptr_t preferredAddress, const uint8_t* expectedBytes,
            size_t expectedByteCount, void* detour, void** original) {
    void* target = reinterpret_cast<void*>(game_image::Rebase(preferredAddress));

    if (std::memcmp(target, expectedBytes, expectedByteCount) != 0) {
        log::Writef("mod", "Hook %s: unexpected bytes at target, so it was not installed", name);
        return false;
    }
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        log::Writef("mod", "Hook %s: %s", name, MH_StatusToString(status));
        return false;
    }
    return MH_QueueEnableHook(target) == MH_OK;
}

bool EnableAll() {
    MH_STATUS status = MH_ApplyQueued();
    if (status != MH_OK) {
        log::Writef("mod", "Enabling hooks failed: %s", MH_StatusToString(status));
        return false;
    }
    return true;
}

void RemoveAll() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

}  // namespace sgfixes::hooks
