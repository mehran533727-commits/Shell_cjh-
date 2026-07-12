#include "XTFSH/core/builtins.h"
#include "XTFSH/core/executor.h"
#include "XTFSH/core/parser.h"
#include "XTFSH/core/signals.h"
#include "XTFSH/ui/rich_output.h"
#include "XTFSH/util/io.h"
#include "XTFSH/util/limits.h"
#include "XTFSH/util/safe_tmpdir.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unordered_set>
#include <vector>

using namespace std;

// Open a private, unlinked temp file seeded with `body`, positioned at
// offset 0 for reading. Used for stdin heredoc redirection. Returns the
// fd, or -1 on error. Matches the bash/dash/ksh approach and avoids the
// pipe-buffer deadlock for heredoc bodies larger than PIPE_BUF.
//
// Security (deep-review finding #2): TMPDIR is honoured only when it
// is owned by us with mode 0700; otherwise we fall back to /tmp. The
// previous code trusted $TMPDIR unconditionally, which was a
// symlink/TOCTOU race if an attacker could set TMPDIR to a directory
// they also controlled.
//
// Security (deep-review finding #4): body size is capped at
// XTFSH_MAX_HEREDOC_BYTES -- a runaway redirection can't fill /tmp.
int open_heredoc_fd(const std::string &body) {
    if (body.size() > XTFSH::util::XTFSH_MAX_HEREDOC_BYTES) {
        write_stderr("XTFSH: 内嵌文档内容超过最大大小（100 MiB）\n");
        return -1;
    }

    std::string base = XTFSH::util::resolve_safe_tmpdir();
    std::string pattern = base + "/XTFSH-hd-XXXXXX";
    std::vector<char> buf(pattern.begin(), pattern.end());
    buf.push_back('\0');
    int fd = ::mkstemp(buf.data());
    if (fd < 0 && errno == ENOENT && base != "/tmp") {
        // Fallback: resolved tmp dir is invalid for some reason, try /tmp.
        std::string fallback = "/tmp/XTFSH-hd-XXXXXX";
        std::vector<char> fb(fallback.begin(), fallback.end());
        fb.push_back('\0');
        fd = ::mkstemp(fb.data());
        if (fd < 0) return -1;
        buf = std::move(fb);  // for the unlink below
    }
    if (fd < 0) return -1;
    // Unlink immediately: the file is now private to our process tree.
    ::unlink(buf.data());
    // Tighten perms explicitly so umask can't widen the file.
    (void)::fchmod(fd, 0600);
    // CLOEXEC via fcntl (two-step F_GETFD + F_SETFD). Linux offers
    // O_CLOEXEC via mkostemp, but macOS lacks mkostemp as of this writing,
    // so we stick with the portable path.
    int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0) ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

    size_t off = 0;
    while (off < body.size()) {
        ssize_t w = ::write(fd, body.data() + off, body.size() - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return -1;
        }
        off += static_cast<size_t>(w);
    }
    if (::lseek(fd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// Create a pipe with FD_CLOEXEC set on both ends so exec'd children
// that don't close their copy (daemons, misbehaving tools) can't hold
// the other end open past our own close. Prefers the atomic pipe2 on
// Linux; falls back to pipe()+fcntl on macOS, which lacks pipe2.
static int pipe_cloexec(int fds[2]) {
#if defined(__linux__) && defined(O_CLOEXEC)
    return pipe2(fds, O_CLOEXEC);
#else
    if (pipe(fds) < 0) return -1;
    for (int k = 0; k < 2; ++k) {
        int flags = fcntl(fds[k], F_GETFD);
        if (flags >= 0) (void)fcntl(fds[k], F_SETFD, flags | FD_CLOEXEC);
    }
    return 0;
#endif
}

void setup_child_io(const vector<Redirection> &redirections) {
    for (const Redirection &r : redirections) {
        if (r.dup_to_stdout) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
            continue;
        }
        if (r.fd == 0) {
            int in;
            if (r.is_heredoc) {
                in = open_heredoc_fd(r.heredoc_body);
                if (in < 0) {
        write_stderr("XTFSH: 内嵌文档：创建临时文件失败\n");
                    _exit(1);
                }
            } else {
                in = open(r.filename.c_str(), O_RDONLY);
                if (in < 0) {
                    write_stderr("XTFSH: " + r.filename + "：没有此文件或目录\n");
                    _exit(1);
                }
            }
            dup2(in, STDIN_FILENO);
            close(in);
        } else if (r.fd == 1) {
            int out;
            if (r.append) {
                out = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                out = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (out < 0) {
                write_stderr("XTFSH: " + r.filename + "：无法打开文件\n");
                _exit(1);
            }
            dup2(out, STDOUT_FILENO);
            close(out);
        } else if (r.fd == 2) {
            int err = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (err < 0) {
                write_stderr("XTFSH: " + r.filename + "：无法打开文件\n");
                _exit(1);
            }
            dup2(err, STDERR_FILENO);
            close(err);
        }
    }
}

// Commands we never intercept stdout for — they rely on a real TTY
// (cursor addressing, alternate screen, raw input) and a buffering wrapper
// breaks them.
static bool is_interactive_cmd(const std::string &cmd) {
    static const std::unordered_set<std::string> interactive = {
        "vim", "vi", "nvim", "emacs", "nano", "less", "more",
        "man", "top", "htop", "btop", "tmux", "screen", "ssh",
        "fzf", "watch", "mc", "ranger", "tig",
        // 嵌套 shell / REPL：需要真 TTY 才能交互式运行。否则 bash 之类
        // 看到 stderr 不是终端会认定自己"非交互"，既不出提示符也不读
        // ~/.bashrc。收录后，在 XTFSH 里直接敲 bash/sh/… 即可进入子 shell，
        // 无需先 exit——两个 shell 之间随意切换。
        "bash", "sh", "zsh", "fish", "dash", "xtfsh",
    };
    // Basename only (strip path).
    size_t slash = cmd.find_last_of('/');
    std::string base = (slash == std::string::npos) ? cmd : cmd.substr(slash + 1);
    return interactive.count(base) > 0;
}

static bool auto_linkify_enabled() {
    const char *v = getenv("XTFSH_AUTO_LINKIFY");
    return v && *v && string(v) != "0";
}

static string join_command(const vector<string> &argv) {
    string out;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i > 0) out += " ";
        out += argv[i];
    }
    return out;
}

static double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

int register_background_job(pid_t pid, const string &command,
                            ShellState &state) {
    CoreState::BackgroundJob job;
    job.job_id = state.core.next_background_job_id++;
    job.pid = pid;
    job.command = command;
    job.start_time = now_seconds();
    job.running = true;
    state.core.background_processes[pid] = job;

    write_stdout("[任务 " + to_string(job.job_id) + "] 已启动 | 进程号=" +
                 to_string(pid) + " | 命令=\"" + command +
                 "\" | 模式=后台\n");
    return job.job_id;
}

int foreground_process(const vector<string> &argv,
                       const vector<Redirection> &redirections,
                       string *captured_stderr) {
    // Build C-style args for execvp
    vector<const char *> c_args;
    for (const string &a : argv) c_args.push_back(a.c_str());
    c_args.push_back(nullptr);

    // 交互式程序（编辑器/分页器/嵌套 shell/REPL）必须保留真 TTY：一旦
    // 捕获它们的 stderr，bash 之类就会判定自己非交互，起来后没有提示符、
    // 不加载 rc。对这些命令跳过 stderr 捕获（stdout 加工由下面同一判定一并跳过）。
    if (!argv.empty() && is_interactive_cmd(argv[0])) captured_stderr = nullptr;

    int stderr_pipe[2] = {-1, -1};
    if (captured_stderr) {
        captured_stderr->clear();
        if (pipe_cloexec(stderr_pipe) < 0) {
            XTFSH::io::warning("无法捕获标准错误输出");
            captured_stderr = nullptr; // fall back to no capture
        }
    }

    // Optional stdout interception for URL linkification.
    int stdout_pipe[2] = {-1, -1};
    bool intercept_stdout = !argv.empty() &&
                            auto_linkify_enabled() &&
                            !is_interactive_cmd(argv[0]) &&
                            redirections.empty();
    if (intercept_stdout) {
        if (pipe_cloexec(stdout_pipe) < 0) {
            intercept_stdout = false;
        }
    }

    int status;
    pid_t pid = fork();
    if (pid < 0) {
        exit_with_message("Error: Fork failed!\n", 1);
    } else if (pid == 0) {
        // Child
        reset_child_signal_state();
        if (captured_stderr && stderr_pipe[1] >= 0) {
            close(stderr_pipe[0]); // close read end in child
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
        }
        if (intercept_stdout) {
            close(stdout_pipe[0]);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[1]);
        }
        setup_child_io(redirections);
        execvp(c_args[0], const_cast<char *const *>(c_args.data()));
        string err_msg = string(c_args[0]) + ": " + strerror(errno) + "\n";
        write_stderr(err_msg);
        // _exit, not exit: the forked child must not run C++ global
        // destructors (replxx, sqlite, curl, plugin registry) — those
        // hold resources the parent still owns, and tearing them down
        // in the child occasionally segfaults (XTFSH issue: $? = 139
        // instead of 127 on command-not-found, ~1 in 5 on macOS).
        _exit(127);
    } else {
        // Parent
        if (stderr_pipe[1] >= 0) close(stderr_pipe[1]); // close write end
        if (intercept_stdout) close(stdout_pipe[1]);

        fg_child_pid.store(pid, std::memory_order_release);

        auto set_nonblocking = [](int fd) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        };
        auto close_read_fd = [](int &fd) {
            if (fd >= 0) {
                close(fd);
                fd = -1;
            }
        };

        std::string stdout_carry;
        auto drain_stdout = [&]() {
            char buf[4096];
            while (stdout_pipe[0] >= 0) {
                ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
                if (n > 0) {
                    stdout_carry.append(buf, static_cast<size_t>(n));
                    size_t start = 0;
                    while (true) {
                        size_t nl = stdout_carry.find('\n', start);
                        if (nl == std::string::npos) break;
                        std::string line = stdout_carry.substr(start, nl - start + 1);
                        std::string linked = XTFSH::ui::linkify_urls(line);
                        if (write(STDOUT_FILENO, linked.data(), linked.size())) {}
                        start = nl + 1;
                    }
                    stdout_carry.erase(0, start);
                    continue;
                }
                if (n == 0) {
                    close_read_fd(stdout_pipe[0]);
                    break;
                }
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                close_read_fd(stdout_pipe[0]);
                break;
            }
        };

        auto drain_stderr = [&]() {
            char buf[4096];
            while (stderr_pipe[0] >= 0) {
                ssize_t n = read(stderr_pipe[0], buf, sizeof(buf));
                if (n > 0) {
                    if (captured_stderr && captured_stderr->size() < 4096) {
                        size_t room = 4096 - captured_stderr->size();
                        captured_stderr->append(buf, std::min(room, static_cast<size_t>(n)));
                    }
                    if (write(STDERR_FILENO, buf, static_cast<size_t>(n))) {}
                    continue;
                }
                if (n == 0) {
                    close_read_fd(stderr_pipe[0]);
                    break;
                }
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                close_read_fd(stderr_pipe[0]);
                break;
            }
        };

        if (intercept_stdout && stdout_pipe[0] >= 0) set_nonblocking(stdout_pipe[0]);
        if (captured_stderr && stderr_pipe[0] >= 0) set_nonblocking(stderr_pipe[0]);

        while ((intercept_stdout && stdout_pipe[0] >= 0) ||
               (captured_stderr && stderr_pipe[0] >= 0)) {
            pollfd fds[2];
            int nfds = 0;
            int stdout_slot = -1;
            int stderr_slot = -1;
            if (intercept_stdout && stdout_pipe[0] >= 0) {
                stdout_slot = nfds;
                fds[nfds++] = {stdout_pipe[0], POLLIN | POLLHUP | POLLERR, 0};
            }
            if (captured_stderr && stderr_pipe[0] >= 0) {
                stderr_slot = nfds;
                fds[nfds++] = {stderr_pipe[0], POLLIN | POLLHUP | POLLERR, 0};
            }

            int pr = poll(fds, nfds, -1);
            if (pr < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (stdout_slot >= 0 && fds[stdout_slot].revents) drain_stdout();
            if (stderr_slot >= 0 && fds[stderr_slot].revents) drain_stderr();
        }

        if (!stdout_carry.empty()) {
            std::string linked = XTFSH::ui::linkify_urls(stdout_carry);
            if (write(STDOUT_FILENO, linked.data(), linked.size())) {}
        }

        waitpid(pid, &status, WUNTRACED);
        fg_child_pid.store(0, std::memory_order_release);

        // Check exit status properly
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return 0; // stopped
    }
    return 1;
}

void background_process(const vector<string> &argv,
                        ShellState &state,
                        const vector<Redirection> &redirections) {
    if (argv.size() < 2) {
        XTFSH::io::error("bg: 用法：bg <command> [args...]");
        return;
    }
    vector<string> command(argv.begin() + 1, argv.end());
    background_command(command, state, redirections);
}

void background_command(const vector<string> &argv,
                        ShellState &state,
                        const vector<Redirection> &redirections) {
    if (argv.empty()) {
        XTFSH::io::error("background: 缺少命令");
        return;
    }
    if ((int)state.core.background_processes.size() >= state.core.max_background_processes) {
        XTFSH::io::error("后台进程数量已达到上限");
        return;
    }

    vector<const char *> c_args;
    for (const string &arg : argv) c_args.push_back(arg.c_str());
    c_args.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        exit_with_message("Error: Fork failed!\n", 1);
    } else if (pid == 0) {
        reset_child_signal_state();
        setup_child_io(redirections);
        execvp(c_args[0], const_cast<char *const *>(c_args.data()));
        string err_msg = string(c_args[0]) + ": " + strerror(errno) + "\n";
        write_stderr(err_msg);
        _exit(127);  // see note above foreground exec — skip C++ dtors in child
    } else {
        string display = join_command(argv);
        register_background_job(pid, display, state);
        XTFSH::io::debug("后台任务：已创建进程，进程号=" + to_string(pid) +
                        "，命令='" + display + "'");
    }
}

void check_background_process_finished(
    unordered_map<pid_t, CoreState::BackgroundJob> &background_processes) {
    // Drain all ready children in one call. Unix signals coalesce —
    // if 5 SIGCHLDs arrive while blocked, only one delivery is
    // observable. Previously this reaped a single pid per invocation,
    // which left zombies in the tracking map after batch completions
    // and eventually tripped the max-bg cap. Loop with WNOHANG until
    // waitpid reports no-more-ready.
    while (true) {
        int status;
        pid_t pid_finished = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED);
        if (pid_finished <= 0) break;
        auto it = background_processes.find(pid_finished);
        if (it == background_processes.end()) continue;
        CoreState::BackgroundJob &job = it->second;
        if (WIFCONTINUED(status)) {
            job.running = true;
            write_stdout("[任务 " + to_string(job.job_id) + "] 已继续 | 进程号=" +
                         to_string(pid_finished) + "\n");
        } else if (WIFSTOPPED(status)) {
            job.running = false;
            write_stdout("[任务 " + to_string(job.job_id) + "] 已停止 | 进程号=" +
                         to_string(pid_finished) + "\n");
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status)
                                              : 128 + WTERMSIG(status);
            double elapsed = now_seconds() - job.start_time;
            stringstream msg;
            msg << fixed << setprecision(2);
            msg << "[任务 " << job.job_id << "] 已完成 | 进程号="
                << pid_finished << " | 退出码=" << exit_code
                << " | 耗时=" << elapsed << " 秒\n";
            XTFSH::io::debug("后台任务：已回收进程，进程号=" + to_string(pid_finished) +
                            "，退出码=" + to_string(exit_code));
            background_processes.erase(it);
            write_stdout(msg.str());
        }
    }
}

void reap_background_processes(
    unordered_map<pid_t, CoreState::BackgroundJob> &background_processes) {
    while (sigchld_received) {
        sigchld_received = 0;
        size_t before = background_processes.size();
        check_background_process_finished(background_processes);
        size_t reaped = before - background_processes.size();
        if (reaped > 0) {
            XTFSH::io::debug("SIGCHLD：正在回收 " + to_string(reaped) + " 个子进程");
        }
    }
}

int execute_pipeline(vector<PipelineSegment> &segments, ShellState *state) {
    int num_cmds = (int)segments.size();
    vector<int> pipefds(2 * (num_cmds - 1));
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe_cloexec(&pipefds[2 * i]) < 0) {
            exit_with_message("Error: Pipe creation failed!\n", 1);
        }
    }

    vector<pid_t> pids(num_cmds);
    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            exit_with_message("Error: Fork failed!\n", 1);
        } else if (pids[i] == 0) {
            reset_child_signal_state();
            // Wire pipe stdin/stdout first so per-segment redirections
            // (applied after) can still override (e.g. 2>/dev/null).
            if (i > 0) {
                dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            }
            if (i < num_cmds - 1) {
                dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            }
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }
            setup_child_io(segments[i].redirections);

            // Subshell segment: parse + run the inner source as a
            // command line, exit with its last status.
            if (!segments[i].subshell_body.empty()) {
                ShellState fallback;
                ShellState &st = state ? *state : fallback;
                // Same rationale as executor.cpp subshell fork: the child
                // must not record history while sharing a post-fork SQLite
                // connection with the parent.
                st.exec.in_subshell = true;
                std::vector<CommandSegment> segs =
                    parse_command_line(segments[i].subshell_body);
                execute_command_line(segs, st);
                // Flush stdio before _exit — the child may have buffered
                // echo output (stdout is a pipe → fully buffered). _exit
                // bypasses libc cleanup, so we must flush explicitly.
                std::fflush(nullptr);
                _exit(st.core.last_exit_status);
            }

            // Normal segment: builtin or external.
            const vector<string> &argv = segments[i].argv;
            if (argv.empty()) { std::fflush(nullptr); _exit(0); }
            const auto &builtins = get_builtins();
            auto bit = builtins.find(argv[0]);
            if (bit != builtins.end()) {
                ShellState fallback;
                ShellState &st = state ? *state : fallback;
                int rc = bit->second(argv, st);
                std::fflush(nullptr);
                _exit(rc);
            }
            vector<const char *> c_args;
            for (const string &a : argv) c_args.push_back(a.c_str());
            c_args.push_back(nullptr);
            execvp(c_args[0], const_cast<char *const *>(c_args.data()));
            string err_msg = string(c_args[0]) + ": " + strerror(errno) + "\n";
            if (write(STDERR_FILENO, err_msg.c_str(), err_msg.size())) {}
            _exit(127);
        }
    }

    for (int j = 0; j < 2 * (num_cmds - 1); j++) {
        close(pipefds[j]);
    }

    int last_status = 0;
    for (int i = 0; i < num_cmds; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == num_cmds - 1) {
            if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        }
    }
    return last_status;
}
