// Background-job builtins: bglist, bgkill, bgstop, bgstart, fg.

#include "CJHSH/builtins.h"
#include "CJHSH/core/executor.h"
#include "CJHSH/core/signals.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>

using namespace std;

namespace {

pid_t get_background_job_pid(
    unordered_map<pid_t, CoreState::BackgroundJob> &background_processes,
    int job_id) {
    for (auto &process : background_processes) {
        if (process.second.job_id == job_id) return process.first;
    }
    return -1;
}

int parse_job_number(const vector<string> &argv, const string &cmd_name) {
    if (argv.size() < 2) {
        write_stderr(cmd_name + ": missing process number\n");
        return -1;
    }
    try {
        return stoi(argv[1]);
    } catch (const invalid_argument&) {
        write_stderr(cmd_name + ": invalid process number\n");
        return -1;
    } catch (const out_of_range&) {
        write_stderr(cmd_name + ": process number out of range\n");
        return -1;
    }
}

} // anonymous namespace

int builtin_bglist(const vector<string> &, ShellState &state) {
    reap_background_processes(state.core.background_processes);

    vector<CoreState::BackgroundJob> jobs;
    for (auto &process : state.core.background_processes) {
        jobs.push_back(process.second);
    }
    sort(jobs.begin(), jobs.end(),
         [](const auto &a, const auto &b) { return a.job_id < b.job_id; });

    int running = 0;
    for (const auto &job : jobs) {
        if (job.running) ++running;
        stringstream ss;
        ss << "[JOB " << job.job_id << "] "
           << (job.running ? "running" : "stopped")
           << " | pid=" << job.pid
           << " | cmd=\"" << job.command << "\"\n";
        write_stdout(ss.str());
    }

    stringstream ss;
    ss << "Total Background Jobs: " << jobs.size()
       << " (" << running << " running)" << endl;
    write_stdout(ss.str());
    return 0;
}

int builtin_bgkill(const vector<string> &argv, ShellState &state) {
    int n = parse_job_number(argv, "bgkill");
    if (n < 0) return 1;
    pid_t pid = get_background_job_pid(state.core.background_processes, n);
    if (pid == -1) { write_stderr("bgkill: invalid job number\n"); return 1; }
    if (kill(pid, SIGTERM) == -1) { write_stderr(string(strerror(errno)) + "\n"); return 1; }
    return 0;
}

int builtin_bgstop(const vector<string> &argv, ShellState &state) {
    int n = parse_job_number(argv, "bgstop");
    if (n < 0) return 1;
    pid_t pid = get_background_job_pid(state.core.background_processes, n);
    if (pid == -1) { write_stderr("bgstop: invalid job number\n"); return 1; }
    if (kill(pid, SIGSTOP) == -1) { write_stderr(string(strerror(errno)) + "\n"); return 1; }
    return 0;
}

int builtin_bgstart(const vector<string> &argv, ShellState &state) {
    int n = parse_job_number(argv, "bgstart");
    if (n < 0) return 1;
    pid_t pid = get_background_job_pid(state.core.background_processes, n);
    if (pid == -1) { write_stderr("bgstart: invalid job number\n"); return 1; }
    if (kill(pid, SIGCONT) == -1) { write_stderr(string(strerror(errno)) + "\n"); return 1; }
    return 0;
}

int builtin_fg(const vector<string> &argv, ShellState &state) {
    if (state.core.background_processes.empty()) {
        write_stderr("fg: no background jobs\n");
        return 1;
    }
    pid_t pid;
    if (argv.size() < 2) {
        pid = -1;
        int best_job_id = -1;
        for (auto &p : state.core.background_processes) {
            if (p.second.job_id > best_job_id) {
                best_job_id = p.second.job_id;
                pid = p.first;
            }
        }
    } else {
        int n;
        try { n = stoi(argv[1]); }
        catch (const invalid_argument&) { write_stderr("fg: invalid job number\n"); return 1; }
        catch (const out_of_range&) { write_stderr("fg: job number out of range\n"); return 1; }
        pid = get_background_job_pid(state.core.background_processes, n);
        if (pid == -1) { write_stderr("fg: invalid job number\n"); return 1; }
    }
    kill(pid, SIGCONT);
    fg_child_pid.store(pid, std::memory_order_release);
    int status;
    waitpid(pid, &status, WUNTRACED);
    fg_child_pid.store(0, std::memory_order_release);
    state.core.background_processes.erase(pid);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
