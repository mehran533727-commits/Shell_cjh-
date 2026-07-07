#ifndef CJHSH_REPL_H
#define CJHSH_REPL_H

#include "CJHSH/shell.h"
#include "replxx.hxx"

namespace CJHSH {

// Enter the interactive read-eval-print loop: print the banner, set up
// replxx (keybindings, highlighter, hints), read lines, dispatch to the
// executor. Returns when the user exits or Ctrl-Ds twice.
int run_interactive(ShellState &state);

} // namespace CJHSH

// Expand the `!!` / `!n` history-bang notation against replxx's
// in-memory ring. REPL-only (the parser doesn't know about the ring),
// so this declaration lives here rather than in CJHSH/core/parser.h —
// that way every TU pulling parser.h doesn't transitively pull replxx.
std::string expand_history_bang(const std::string &line, replxx::Replxx &rx);

#endif // CJHSH_REPL_H
