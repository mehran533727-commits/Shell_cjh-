#ifndef CJHSH_UTIL_CONFIG_RESOLVER_H
#define CJHSH_UTIL_CONFIG_RESOLVER_H

// Centralised resolver for every filesystem path CJHSH reads or writes.
//
// Precedence (high → low) for config dir:
//   1. $CJHSH_CONFIG_HOME           — CJHSH-specific override
//   2. $XDG_CONFIG_HOME/CJHSH       — XDG Base Directory spec
//   3. $HOME/.config/CJHSH          — historical default
//
// Precedence for data dir:
//   1. $CJHSH_DATA_HOME             — CJHSH-specific override
//   2. $XDG_DATA_HOME/CJHSH         — XDG Base Directory spec
//   3. $HOME/.CJHSH                 — legacy default (preserved so
//                                    existing installs keep working)
//
// Every caller that touches the filesystem should go through this module
// so tests can redirect by setting $HOME / $CJHSH_*_HOME and so future
// XDG / Windows support doesn't require touching 12 files again.

#include <string>

namespace CJHSH::config {

// ── Base directories ──────────────────────────────────────────

// User-editable configuration (themes, CJHSHrc, completions specs).
std::string get_config_dir();

// Shell-owned state (databases, sessions, caches).
std::string get_data_dir();

// ── Path helpers ──────────────────────────────────────────────

// $HOME/.CJHSHrc — kept at the conventional location regardless of
// XDG so users familiar with bashrc/zshrc don't have to hunt for it.
std::string get_CJHSHrc_path();

// $HOME/.CJHSH_history — replxx's line-history dump.
std::string get_history_file_path();

// $HOME/.CJHSH_z — frecency database.
std::string get_frecency_path();

// <config>/theme.toml — the currently active theme file (copied from a
// bundled one by `theme set`).
std::string get_theme_toml_path();

// <config>/theme.name — the basename of the active theme.
std::string get_theme_name_path();

// <config>/themes — user theme directory (scanned in addition to the
// compile-time CJHSH_THEMES_DIR).
std::string get_user_themes_dir();

// <data>/sessions — saved session files for the session builtin.
std::string get_sessions_dir();

// <data>/history.db — SQLite history backing store.
std::string get_history_db_path();

// <config>/completions/fig — fig-format completion specs.
std::string get_fig_completions_dir();

// ~/.config/starship.toml — starship's own convention; left alone.
std::string get_starship_config_path();

// ── Utilities ─────────────────────────────────────────────────

// Create `path` and every missing parent directory. Returns true on
// success or when the directory already exists.
bool ensure_dir(const std::string &path);

} // namespace CJHSH::config

#endif // CJHSH_UTIL_CONFIG_RESOLVER_H
