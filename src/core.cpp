// The beginning of the program, and the interfaces that need no table: abort,
// stream, env, time and memory.
#include "common.h"

namespace oks {
int    g_argc = 0;
char** g_argv = nullptr;
char** g_envp = nullptr;

const char* variable(const char* name, unsigned long n) {
    for (char** e = g_envp; e && *e; ++e)
        if (same(*e, name, n) && (*e)[n] == '=') return *e + n + 1;
    return nullptr;
}
}  // namespace oks

using oks::word;

// ================================================================== the start
//
// THE STANDALONE ARRANGEMENT (feature `standalone') IS THE ONLY ONE: a program
// of this implementation links no file of the system, so nothing else has
// received control. The kernel enters the image with the argument block at the
// stack pointer --- argc, the arguments, a null, the environment, a null. A C
// library written above openkal is handed the program (`__libc_start_main',
// openkal-musl's); with none, the image's initialisers run and `main' is called.
#ifdef OKS_STANDALONE
extern "C" {
int main(int, char**, char**);
[[gnu::weak]] int __libc_start_main(int (*)(int, char**, char**), int, char**, void (*)(), void (*)(), void (*)());
[[noreturn]] void __oks_start_c(long* block);
}
namespace {
using initialiser = void (*)(int, char**, char**);
extern "C" {
[[gnu::weak]] extern initialiser __preinit_array_start[];
[[gnu::weak]] extern initialiser __preinit_array_end[];
[[gnu::weak]] extern initialiser __init_array_start[];
[[gnu::weak]] extern initialiser __init_array_end[];
}
}  // namespace

__asm__(
".text\n"
".globl _start\n"
".type _start,@function\n"
"_start:\n"
"  xorl %ebp,%ebp\n"
"  movl %esp,%eax\n"
"  andl $-16,%esp\n"
"  subl $12,%esp\n"
"  pushl %eax\n"
"  call __oks_start_c\n"
"  hlt\n"
".size _start,.-_start\n");

extern "C" [[noreturn]] void __oks_start_c(long* block) {
    const int argc = static_cast<int>(block[0]);
    char** argv = reinterpret_cast<char**>(block + 1);
    char** envp = argv + argc + 1;
    oks::g_argc = argc; oks::g_argv = argv; oks::g_envp = envp;
    // A write to a stream whose reader has gone answers `closed' (EPIPE) rather
    // than ending the program by a signal it did not ask for.
    oks::call(oks::nr::signal, oks::SIGPIPE, oks::SIG_IGN);
    if (__libc_start_main != nullptr) {
        __libc_start_main(main, argc, argv, nullptr, nullptr, nullptr);
        kal_exit(127);   // a C library's hand-over does not return
    }
    for (initialiser* p = __preinit_array_start; p != __preinit_array_end; ++p) (*p)(argc, argv, envp);
    for (initialiser* p = __init_array_start;    p != __init_array_end;    ++p) (*p)(argc, argv, envp);
    kal_exit(main(argc, argv, envp));
}
#endif  // OKS_STANDALONE

extern "C" {

// ==================================================================== version
//
// The interfaces are the manifest's list (`[kernel-abi] provides-interfaces');
// the suite compares the two and the names the artefact defines.
kal_u64 kal_version(void) { return KAL_VERSION; }
kal_u64 kal_interfaces(void) {
    return KAL_IFACE_ABORT | KAL_IFACE_STREAM | KAL_IFACE_MEMORY | KAL_IFACE_ENV
         | KAL_IFACE_TIME | KAL_IFACE_FS | KAL_IFACE_TERMINAL;
}

// ====================================================================== abort
[[noreturn]] void kal_exit(int code) {
    for (;;) oks::call(oks::nr::exit, code);
}
[[noreturn]] void kal_abort(const char* msg, kal_uintptr len) {
    if (msg && len) {
        oks::call(oks::nr::write, 2, oks::arg(msg), static_cast<word>(len));
        oks::call(oks::nr::write, 2, oks::arg("\n"), 1);
    }
    // SIGABRT at its default disposition, sent to this program; one that
    // survived it ends with the status of an abort.
    oks::call(oks::nr::signal, oks::SIGABRT, oks::SIG_DFL);
    const auto self = oks::call(oks::nr::getpid);
    oks::call(oks::nr::kill, self.value, oks::SIGABRT);
    kal_exit(134);
}

// ===================================================================== stream
kal_stream kal_stdin (void) { return kal_stream{ oks::pack(0) }; }
kal_stream kal_stdout(void) { return kal_stream{ oks::pack(1) }; }
kal_stream kal_stderr(void) { return kal_stream{ oks::pack(2) }; }

kal_intptr kal_stream_write(kal_stream s, const void* buf, kal_uintptr len) {
    const int fd = oks::unpack(s.h);
    if (fd < 0) return -kal_err_invalid;
    for (;;) {
        const auto r = oks::call(oks::nr::write, fd, oks::arg(buf), static_cast<word>(len));
        if (r.error == oks::EINTR) continue;
        return r.failed() ? -static_cast<kal_intptr>(oks::translate(r.error)) : static_cast<kal_intptr>(r.value);
    }
}
kal_intptr kal_stream_read(kal_stream s, void* buf, kal_uintptr len) {
    const int fd = oks::unpack(s.h);
    if (fd < 0) return -kal_err_invalid;
    for (;;) {
        const auto r = oks::call(oks::nr::read, fd, oks::arg(buf), static_cast<word>(len));
        if (r.error == oks::EINTR) continue;
        return r.failed() ? -static_cast<kal_intptr>(oks::translate(r.error)) : static_cast<kal_intptr>(r.value);
    }
}
int kal_stream_flush(kal_stream s) { return oks::unpack(s.h) < 0 ? kal_err_invalid : kal_ok; }
kal_uintptr kal_stream_props(kal_stream s) {
    const int fd = oks::unpack(s.h);
    oks::ktermio t{};
    return (fd >= 0 && !oks::call(oks::nr::ioctl, fd, oks::TCGETA, oks::arg(&t)).failed()) ? KAL_STREAM_PROP_INTERACTIVE : 0;
}

// ======================================================================== env
kal_uintptr kal_env_arg_count(void) { return static_cast<kal_uintptr>(oks::g_argc); }
kal_intptr kal_env_arg(kal_uintptr i, char* out, kal_uintptr cap) {
    if (i >= static_cast<kal_uintptr>(oks::g_argc)) return -kal_err_invalid;
    return oks::put(oks::g_argv[i], oks::length(oks::g_argv[i]), out, cap);
}
kal_intptr kal_env_var(const char* name, kal_uintptr n, char* out, kal_uintptr cap) {
    if (!name || n == 0) return -kal_err_invalid;
    const char* v = oks::variable(name, n);
    if (!v) return -kal_err_not_found;
    return oks::put(v, oks::length(v), out, cap);
}
kal_uintptr kal_env_var_count(void) {
    kal_uintptr n = 0;
    for (char** e = oks::g_envp; e && *e; ++e) ++n;
    return n;
}
kal_intptr kal_env_var_at(kal_uintptr i, char* out, kal_uintptr cap) {
    kal_uintptr n = 0;
    for (char** e = oks::g_envp; e && *e; ++e, ++n)
        if (n == i) return oks::put(*e, oks::length(*e), out, cap);
    return -kal_err_invalid;
}
// No call of these systems names the image a program was started from, and a
// program built for its environment has no loader.
kal_intptr kal_env_image(char*, kal_uintptr) { return -kal_err_not_found; }
kal_intptr kal_env_loader(char*, kal_uintptr) { return -kal_err_not_found; }
kal_intptr kal_env_system(char* out, kal_uintptr cap) { return oks::put("svr4", 4, out, cap); }
int kal_env_resources(void) { return KAL_RESOURCES_BALANCED; }

// ======================================================================= time
//
// THE CLOCKS ARE THE ONES EVERY ONE OF THESE SYSTEMS HAS: `time', in seconds,
// and `times', whose result is the ticks since a point in the past the system
// chose. The granularities are therefore a second and a tick, and are stated.
kal_duration kal_time_wall(void) {
    const auto r = oks::call(oks::nr::time, 0);
    return r.failed() ? 0 : static_cast<kal_duration>(static_cast<unsigned long>(r.value)) * 1000000000ull;
}
kal_duration kal_time_monotonic(void) {
    static kal_duration last = 0;
    oks::ktms t{};
    const auto r = oks::call(oks::nr::times, oks::arg(&t));
    if (!r.failed()) {
        const kal_duration now = static_cast<kal_duration>(static_cast<unsigned long>(r.value)) * (1000000000ull / oks::HZ);
        if (now > last) last = now;      // the counter wraps; the clock does not go back
    }
    return last;
}
kal_duration kal_time_monotonic_granularity(void) { return 1000000000ull / oks::HZ; }
void kal_time_sleep(kal_duration ns) {
    // `poll' of no descriptor is the wait these systems share, in milliseconds.
    kal_duration ms = (ns + 999999ull) / 1000000ull;
    while (ms) {
        const word step = ms > 60000 ? 60000 : static_cast<word>(ms);
        const kal_duration before = kal_time_monotonic();
        const auto r = oks::call(oks::nr::poll, 0, 0, step);
        if (r.failed() && r.error != oks::EINTR) return;
        if (r.error == oks::EINTR) {
            const kal_duration waited = (kal_time_monotonic() - before) / 1000000ull;
            ms = waited >= ms ? 0 : ms - waited;
            continue;
        }
        ms -= static_cast<kal_duration>(step);
    }
}
kal_uintptr kal_time_props(void) { return KAL_TIME_PROP_WALL_AVAILABLE; }

// ===================================================================== memory
//
// MEMORY IS A PRIVATE MAPPING OF THE ZERO DEVICE: Release 4's `mmap' has no
// anonymous mapping of its own, and `/dev/zero' is what its C library maps.
// A request is rounded to pages; an alignment above a page is met by mapping
// the excess and returning both ends.
namespace {
constexpr unsigned long kPage = 4096;
int zero_device() {
    static int fd = -2;
    if (fd == -2) {
        const auto r = oks::call(oks::nr::open, oks::arg("/dev/zero"), oks::O_RDWR, 0);
        fd = r.failed() ? -1 : static_cast<int>(r.value);
        if (fd >= 0) oks::close_on_exec(fd);
    }
    return fd;
}
unsigned long pages(unsigned long n) { return (n + kPage - 1) & ~(kPage - 1); }
}  // namespace

kal_uintptr kal_memory_granularity(void) { return kPage; }
void* kal_alloc(kal_uintptr size, kal_uintptr align) {
    if (size == 0 || (align & (align - 1)) != 0) return nullptr;
    const int fd = zero_device();
    if (fd < 0) return nullptr;
    const unsigned long want = pages(size);
    const unsigned long extra = align > kPage ? align : 0;
    if (want + extra < want) return nullptr;
    const auto r = oks::call(oks::nr::mmap, 0, static_cast<word>(want + extra), oks::PROT_READ | oks::PROT_WRITE,
                             oks::MAP_PRIVATE, fd, 0);
    if (r.failed()) return nullptr;
    const unsigned long at = static_cast<unsigned long>(r.value);
    if (!extra) return reinterpret_cast<void*>(at);
    const unsigned long start = (at + align - 1) & ~(align - 1);
    if (start > at) oks::call(oks::nr::munmap, static_cast<word>(at), static_cast<word>(start - at));
    const unsigned long end = at + want + extra;
    if (end > start + want) oks::call(oks::nr::munmap, static_cast<word>(start + want), static_cast<word>(end - start - want));
    return reinterpret_cast<void*>(start);
}
void kal_free(void* p, kal_uintptr size, kal_uintptr) {
    if (p && size) oks::call(oks::nr::munmap, oks::arg(p), static_cast<word>(pages(size)));
}
int kal_memory_info(kal_memory_status* out) {
    if (!out) return kal_err_invalid;
    const kal_u32 cap = out->self_size;
    if (cap < 2 * sizeof(kal_u32)) return kal_err_invalid;
    kal_memory_status s{}; s.self_size = cap; s.present = 0;
    // The machine's memory, as `sysi86' answers it (SI86MEM); what is available
    // now no call of the shared part states.
    const auto r = oks::call(oks::nr::sysi86, 65);
    // The answer is a word: a machine of more memory than a word counts states what the word holds.
    if (!r.failed() && r.value != 0 && cap >= __builtin_offsetof(kal_memory_status, total_bytes) + 8) {
        s.present |= KAL_MSTATUS_TOTAL;
        s.total_bytes = static_cast<kal_u64>(static_cast<unsigned long>(r.value));
    }
    oks::copy(out, &s, cap < sizeof s ? cap : sizeof s);
    return kal_ok;
}

}  // extern "C"
