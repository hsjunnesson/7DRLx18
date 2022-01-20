namespace game {

class ActionBinds;
enum ActionBindEntry_Action : int;
enum ActionBindEntry_Bind : int;

// Make sure there are no duplicate binds or actions in the action binds. Logs errors to the console.
// Returns true on valid.
bool validate_actionbinds(const ActionBinds &action_binds);

// Returns the ActionBindEntry_Action corresponding to a particlar bind value.
// Defaults to ActionBindEntry_Action_ACTION_UNKNOWN if none found.
// Assumes a valid ActionBinds with no duplicate binds or actions.
ActionBindEntry_Action action_for_bind(const ActionBinds &action_binds, const ActionBindEntry_Bind bind);

}
