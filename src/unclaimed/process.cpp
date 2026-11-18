// openkal.process on `fork', `execve', `wait' and `kill'.
//
// NOT BUILT, AND NOT CLAIMED, IN 0.1 (mcpp.toml: the interface is absent from
// the list and this file from the sources). What is here was run where the
// interface is emulated on another kernel: a pipe, a second program and its
// ending hold there, and the replacement of an image does not --- that host
// starts the file as its own kind of program, outside the emulation --- so the
// suite's observations of a started program cannot be made. The file is kept
// for the system itself, where they can; until then no name of the interface
// is defined (openkal clause 6.1).
#include "../common.h"

namespace {
using oks::word;

// THE PROGRAMS THIS ONE STARTED. `wait' answers for whichever ended, so an
// ending is recorded here until the program that started it asks.
struct started { int pid; bool used; bool ended; int status; };
constexpr int kStarted = 64;
started g_started[kStarted];

started* of(kal_process p) {
    if (p.h == 0 || p.h > static_cast<kal_uintptr>(kStarted)) return nullptr;
    started* s = &g_started[p.h - 1];
    return s->used ? s : nullptr;
}
void record(int pid, int status) {
    for (auto& s : g_started) if (s.used && !s.ended && s.pid == pid) { s.ended = true; s.status = status; return; }
}

// One block holding a vector of terminated strings: openkal's are bytes and a length.
char** vector_of(const char** v, const kal_uintptr* lens, kal_uintptr n, kal_uintptr* size) {
    kal_uintptr bytes = (n + 1) * sizeof(char*);
    for (kal_uintptr i = 0; i < n; ++i) bytes += lens[i] + 1;
    auto** out = static_cast<char**>(kal_alloc(bytes, alignof(char*)));
    if (!out) return nullptr;
    char* at = reinterpret_cast<char*>(out + n + 1);
    for (kal_uintptr i = 0; i < n; ++i) {
        out[i] = at;
        oks::copy(at, v[i], lens[i]); at[lens[i]] = 0;
        at += lens[i] + 1;
    }
    out[n] = nullptr;
    *size = bytes;
    return out;
}

[[noreturn]] void child(const kal_spawn* how, const char* path, char** av, char** ev, int fds[3], int report) {
    // A stream placed at a position another position's stream still occupies is
    // moved above the three first, so no placement closes one not yet placed.
    for (int i = 0; i < 3; ++i)
        if (fds[i] >= 0 && fds[i] < 3 && fds[i] != i) {
            const auto d = oks::call(oks::nr::fcntl, fds[i], oks::F_DUPFD, 3);
            if (!d.failed()) fds[i] = static_cast<int>(d.value);
        }
    for (int i = 0; i < 3; ++i)
        if (fds[i] >= 0 && fds[i] != i) {
            oks::call(oks::nr::close, i);
            oks::call(oks::nr::fcntl, fds[i], oks::F_DUPFD, i);
        }
    if (how->job) oks::call(oks::nr::pgrpsys, 5, 0, static_cast<word>(how->job->h));   // setpgid
    const char* work = oks::dir_path(how->work);
    if (work && !(work[0] == '.' && work[1] == 0)) oks::call(oks::nr::chdir, oks::arg(work));
    const auto r = oks::call(oks::nr::execve, oks::arg(path), oks::arg(av), oks::arg(ev));
    const int e = static_cast<int>(r.error);
    oks::call(oks::nr::write, report, oks::arg(&e), sizeof e);
    for (;;) oks::call(oks::nr::exit, 127);
}

}  // namespace

extern "C" {

kal_uintptr kal_process_props(void) {
    return KAL_PROCESS_PROP_TERMINATE | KAL_PROCESS_PROP_STREAM_PASSING | KAL_PROCESS_PROP_EXIT_STATUS
         | KAL_PROCESS_PROP_NULL_STREAMS | KAL_PROCESS_PROP_JOB;
}

int kal_process_spawn(const kal_spawn* how, const char* path, kal_uintptr path_len,
                      const char** argv, const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp, const kal_uintptr* envp_lens, kal_uintptr envc,
                      const kal_spawn_streams* streams, kal_process* out) {
    if (!how || !path || !out || path_len == 0) return kal_err_invalid;
    if (how->grant_count) return kal_err_not_supported;
    if (how->flags & (KAL_SPAWN_BOUND_LIFETIME | KAL_SPAWN_ONE_STRING | KAL_SPAWN_CONFINE)) return kal_err_not_supported;
    const kal_uintptr nulls[3] = { KAL_SPAWN_NULL_IN, KAL_SPAWN_NULL_OUT, KAL_SPAWN_NULL_ERR };
    if (streams) {
        const kal_uintptr placed[3] = { streams->in.h, streams->out.h, streams->err.h };
        for (int i = 0; i < 3; ++i) if (placed[i] && (how->flags & nulls[i])) return kal_err_invalid;
    }
    if ((argc && (!argv || !argv_lens)) || (envc && (!envp || !envp_lens))) return kal_err_invalid;
    if (!oks::dir_path(how->work)) return kal_err_invalid;

    // The program, named beneath `base': a start that cannot happen is reported
    // by its reason before a second program exists.
    char p[oks::kPath];
    if (!oks::join(how->base, path, path_len, p)) return kal_err_invalid;
    oks::kstat st{};
    const auto s = oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st));
    if (s.failed()) return oks::translate(s.error);
    if ((st.st_mode & oks::S_IFMT) == oks::S_IFDIR) return kal_err_is_directory;
    if (!(st.st_mode & 0111)) return kal_err_permission;

    started* slot = nullptr; kal_uintptr index = 0;
    for (int i = 0; i < kStarted && !slot; ++i) if (!g_started[i].used) { slot = &g_started[i]; index = static_cast<kal_uintptr>(i); }
    if (!slot) return kal_err_no_memory;

    kal_uintptr av_size = 0, ev_size = 0;
    char** av = vector_of(argv, argv_lens, argc, &av_size);
    char** ev = (envc == 0 && !envp) ? oks::g_envp : vector_of(envp, envp_lens, envc, &ev_size);
    auto release = [&] {
        if (av) kal_free(av, av_size, alignof(char*));
        if (ev && ev != oks::g_envp) kal_free(ev, ev_size, alignof(char*));
    };
    if (!av || !ev) { release(); return kal_err_no_memory; }

    // A position with no stream placed and no null device keeps the caller's.
    int fds[3] = { -1, -1, -1 };
    if (streams) { fds[0] = oks::unpack(streams->in.h); fds[1] = oks::unpack(streams->out.h); fds[2] = oks::unpack(streams->err.h); }
    int opened[3] = { -1, -1, -1 };
    for (int i = 0; i < 3; ++i) {
        if (!(how->flags & nulls[i])) continue;
        const auto n = oks::call(oks::nr::open, oks::arg("/dev/null"), i == 0 ? oks::O_RDONLY : oks::O_WRONLY, 0);
        if (n.failed()) { for (int k = 0; k < i; ++k) if (opened[k] >= 0) oks::call(oks::nr::close, opened[k]); release(); return oks::translate(n.error); }
        fds[i] = opened[i] = static_cast<int>(n.value);
    }

    // WHY THE START FAILED, WHEN IT DID: a pipe the image's replacement closes.
    // The second program writes the error number of `execve' there; an end of
    // file is a program that began.
    const auto pp = oks::call(oks::nr::pipe);
    int report[2] = { -1, -1 };
    if (!pp.failed()) { report[0] = static_cast<int>(pp.value); report[1] = static_cast<int>(pp.second); oks::close_on_exec(report[0]); oks::close_on_exec(report[1]); }

    const auto f = oks::call(oks::nr::fork);
    // The second program: a non-zero second result on the system itself; a zero
    // first result where a host emulates the interface and clears the second.
    if (!f.failed() && (f.second != 0 || f.value == 0)) child(how, p, av, ev, fds, report[1]);
    for (int k = 0; k < 3; ++k) if (opened[k] >= 0) oks::call(oks::nr::close, opened[k]);
    if (report[1] >= 0) oks::call(oks::nr::close, report[1]);
    release();
    if (f.failed()) { if (report[0] >= 0) oks::call(oks::nr::close, report[0]); return oks::translate(f.error); }

    const int pid = static_cast<int>(f.value);
    int why = 0;
    if (report[0] >= 0) {
        for (;;) {
            const auto r = oks::call(oks::nr::read, report[0], oks::arg(&why), sizeof why);
            if (r.error == oks::EINTR) continue;
            if (r.failed() || r.value != static_cast<word>(sizeof why)) why = 0;
            break;
        }
        oks::call(oks::nr::close, report[0]);
    }
    *slot = started{ pid, true, false, 0 };
    if (why != 0) {
        kal_process gone{ index + 1 };
        int status = 0, terminated = 0;
        kal_process_wait(gone, &status, &terminated);
        kal_process_close(gone);
        return oks::translate(why);
    }
    if (how->job && how->job->h == 0) how->job->h = static_cast<kal_uintptr>(pid);
    *out = kal_process{ index + 1 };
    return kal_ok;
}

int kal_process_wait(kal_process p, int* status, int* terminated) {
    started* s = of(p);
    if (!s) return kal_err_invalid;
    while (!s->ended) {
        // The identifier in the first result, the status in the second.
        const auto r = oks::call(oks::nr::wait, 0);
        if (r.error == oks::EINTR) continue;
        if (r.failed()) return oks::translate(r.error);
        record(static_cast<int>(r.value), static_cast<int>(r.second));
    }
    const int low = s->status & 0177;
    if (status) *status = low ? 128 + low : (s->status >> 8) & 0377;
    if (terminated) *terminated = low ? 1 : 0;
    return kal_ok;
}
int kal_process_terminate(kal_process p) {
    started* s = of(p);
    if (!s) return kal_err_invalid;
    if (s->ended) return kal_ok;
    const auto r = oks::call(oks::nr::kill, s->pid, oks::SIGKILL);
    return r.failed() && r.error != oks::ESRCH ? oks::translate(r.error) : kal_ok;
}
void kal_process_close(kal_process p) { if (started* s = of(p)) s->used = false; }

// A JOB IS A PROCESS GROUP (`pgrpsys', whose fifth function is setpgid): a
// program that enters one with a zero handle forms it and is its leader.
int kal_process_job_enter(kal_job* j) {
    if (!j) return kal_err_invalid;
    const auto r = oks::call(oks::nr::pgrpsys, 5, 0, static_cast<word>(j->h));
    if (r.failed()) return oks::translate(r.error);
    if (j->h == 0) j->h = static_cast<kal_uintptr>(oks::call(oks::nr::getpid).value);
    return kal_ok;
}
int kal_process_job_terminate(kal_job j) {
    if (!j.h) return kal_err_invalid;
    const auto r = oks::call(oks::nr::kill, -static_cast<word>(j.h), oks::SIGKILL);
    return r.failed() ? oks::translate(r.error) : kal_ok;
}
void kal_process_job_close(kal_job) {}
// What the properties do not claim reports so (openkal clause 6.2).
int kal_process_job_create(kal_job*, char*, kal_uintptr, kal_uintptr*) { return kal_err_not_supported; }
int kal_process_job_open(const char*, kal_uintptr, kal_job*) { return kal_err_not_supported; }
int kal_process_job_observe(kal_job, const char*, kal_uintptr, kal_u32*) { return kal_err_not_supported; }
int kal_process_job_retire(kal_job, const char*, kal_uintptr, const kal_program_identity*, kal_u32*) { return kal_err_not_supported; }
int kal_process_job_usage(kal_job, kal_job_usage*) { return kal_err_not_supported; }
int kal_process_job_limit(kal_job, kal_uintptr, kal_u64) { return kal_err_not_supported; }
int kal_process_channel(kal_stream*, kal_stream*) { return kal_err_not_supported; }
void kal_process_channel_close(kal_stream) {}
const kal_u32* kal_process_stop_requested(void) { static const kal_u32 never = 0; return &never; }
int kal_process_end_as_requested(void) { return kal_err_not_supported; }
int kal_process_self(kal_process*) { return kal_err_not_supported; }
int kal_process_open_id(kal_u64, kal_process*) { return kal_err_not_supported; }
int kal_process_info(kal_process, kal_u32, kal_program_info*) { return kal_err_not_supported; }

}  // extern "C"
