#include "action_binds.h"
#include <engine/log.h>
#include <hash.h>
#include <memory.h>
#include <proto/game.pb.h>
#include <temp_allocator.h>

namespace game {
using namespace foundation;

bool validate_actionbinds(const ActionBinds &action_binds) {
    TempAllocator256 ta;
    Hash<bool> validation_actions(ta);
    Hash<bool> validation_binds(ta);

    bool valid = true;

    for (int i = 0; i < action_binds.action_binds_size(); ++i) {
        ActionBindEntry action_bind_entry = action_binds.action_binds(i);
        ActionBindEntry_Action action = action_bind_entry.action();
        ActionBindEntry_Bind bind = action_bind_entry.bind();

        // TODO: Remove. We're fine with duplicated actions having multiple binds.
        // if (hash::has(validation_actions, action)) {
        //     const google::protobuf::EnumDescriptor *descriptor = action_bind_entry.Action_descriptor();
        //     const char *action_name = descriptor->FindValueByNumber(action_bind_entry.action())->name().c_str();
        //     log_error("Duplicate keybind action for %s", action_name);
        //     valid = false;
        // }

        if (hash::has(validation_binds, bind)) {
            const google::protobuf::EnumDescriptor *descriptor = action_bind_entry.Bind_descriptor();
            const char *bind_name = descriptor->FindValueByNumber(action_bind_entry.bind())->name().c_str();
            log_error("Duplicate keybind action bind for %s", bind_name);
            valid = false;
        }

        hash::set(validation_actions, action, true);
        hash::set(validation_binds, bind, true);
    }

    return valid;
}

ActionBindEntry_Action action_for_bind(const ActionBinds &action_binds, const ActionBindEntry_Bind bind) {
    ActionBindEntry_Action action = ActionBindEntry_Action_ACTION_UNKNOWN;

    for (int i = 0; i < action_binds.action_binds_size(); ++i) {
        const ActionBindEntry &action_bind = action_binds.action_binds(i);
        if (action_bind.bind() == bind) {
            action = action_bind.action();
            break;
        }
    }

    return action;
}

} // namespace game
