#ifndef CJHSH_UTIL_CONFIG_FILE_H
#define CJHSH_UTIL_CONFIG_FILE_H

// Minimal user-config loader for ~/.CJHSH/config.json.
//
// v1 schema (unknown fields are ignored for forward compatibility):
//   {
//     "plugins":  { "disabled": ["safety", "alias-suggest", ...] },
//     "log_level": "info"
//   }
//
// Resolution:
//   - File path is `<data-dir>/config.json`, where `<data-dir>` comes
//     from CJHSH::config::get_data_dir() (defaults to $HOME/.CJHSH).
//   - CJHSH_LOG_LEVEL, when set and non-empty, overrides `log_level`.
//   - Missing file → defaults, silent.
//   - Malformed JSON → warning on stderr + defaults, no throw.

#include <string>
#include <vector>

namespace CJHSH::config {

struct UserConfig {
    std::vector<std::string> disabled_plugins;
    std::string log_level = "info";
};

// Read `<data-dir>/config.json` (if it exists), apply the
// CJHSH_LOG_LEVEL env override, and return the resulting config.
UserConfig load();

// Accessor for the config loaded during startup. Returns the last
// value passed to set_loaded(); defaulted UserConfig if never set.
const UserConfig& loaded();
void set_loaded(UserConfig cfg);

} // namespace CJHSH::config

#endif // CJHSH_UTIL_CONFIG_FILE_H
