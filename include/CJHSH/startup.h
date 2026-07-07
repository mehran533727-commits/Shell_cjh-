#ifndef CJHSH_STARTUP_H
#define CJHSH_STARTUP_H

#include "CJHSH/shell.h"

namespace CJHSH {

// Register every bundled provider (hooks, completion, prompt, history)
// respecting the CJHSH_DISABLE_* env-var opt-out. Safe to call once at
// startup; re-entering would duplicate registrations.
void register_default_plugins();

// Parse ~/.CJHSHrc and execute each non-empty line in `state`. Silent
// no-op when the file doesn't exist.
void load_CJHSHrc(ShellState &state);

// One-shot benchmark mode: time every startup stage, print the report,
// and return exit code 0. Does not enter the REPL.
int run_benchmark_mode();

} // namespace CJHSH

#endif // CJHSH_STARTUP_H
