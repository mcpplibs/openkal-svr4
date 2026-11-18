// What the files of this implementation share: handles, the kernel's error
// numbers in openkal's words, names as the kernel takes them, and the program's
// arguments and environment.
#pragma once

#include "sys.h"
#include <openkal.h>

namespace oks {

// A STREAM OR FILE HANDLE IS A DESCRIPTOR PLUS ONE, so that zero is never valid.
inline kal_uintptr pack(int fd) { return fd < 0 ? 0 : static_cast<kal_uintptr>(fd) + 1u; }
inline int unpack(kal_uintptr h) { return h == 0 ? -1 : static_cast<int>(h - 1u); }

// The kernel's error number, in openkal's words.
inline int translate(word e) {
    switch (e) {
        case 0:            return kal_ok;
        case EAGAIN:       return kal_err_again;
        case ENOMEM:       return kal_err_no_memory;
        case ENOSPC:       return kal_err_no_space;
        case EACCES:
        case EPERM:
        case EROFS:        return kal_err_permission;
        case ENOSYS:
        case EOPNOTSUPP:   return kal_err_not_supported;
        case EPIPE:
        case ECONNRESET:   return kal_err_closed;
        case ENOENT:       return kal_err_not_found;
        case EEXIST:       return kal_err_exists;
        case ENOTEMPTY:    return kal_err_not_empty;
        case EISDIR:       return kal_err_is_directory;
        case ENOTDIR:      return kal_err_not_directory;
        case ENOEXEC:      return kal_err_not_program;
        case EBADF:
        case EINVAL:
        case ENAMETOOLONG:
        case ELOOP:
        case EFAULT:       return kal_err_invalid;
        default:           return kal_err_io;
    }
}

inline unsigned long length(const char* s) { unsigned long n = 0; while (s && s[n]) ++n; return n; }
inline void copy(void* to, const void* from, unsigned long n) {
    auto* d = static_cast<char*>(to); auto* s = static_cast<const char*>(from);
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
}
inline void zero(void* to, unsigned long n) { auto* d = static_cast<char*>(to); for (unsigned long i = 0; i < n; ++i) d[i] = 0; }
inline bool same(const char* a, const char* b, unsigned long n) { for (unsigned long i = 0; i < n; ++i) if (a[i] != b[i]) return false; return true; }

// What an operation that answers a length writes: as much as fits, and the whole length.
inline kal_intptr put(const char* v, kal_uintptr n, char* out, kal_uintptr cap) {
    if (out && cap) copy(out, v, n < cap ? n : cap);
    return static_cast<kal_intptr>(n);
}

constexpr unsigned long kPath = 1024;   // the kernel's PATH_MAX

// A DIRECTORY HANDLE NAMES A DIRECTORY BY ITS PATH. These systems have no call
// that takes a directory's descriptor and a name beneath it, so a handle is an
// index of a table of paths (plus one), and an operation joins the path and the
// name. The first two are the working directory (`.') and the root, which are
// what `kal_fs_preopen' grants.
const char* dir_path(kal_dir d);
// The path of `name' beneath `base', terminated; false when the handle is not
// one, the name is absolute, ascends, holds a zero byte, or the path is too long.
bool join(kal_dir base, const char* name, kal_uintptr len, char* out);

// The program's arguments and environment, as its beginning was given them.
extern int    g_argc;
extern char** g_argv;
extern char** g_envp;
const char* variable(const char* name, unsigned long n);

// openkal.process's own, for the descriptors a started program must not keep.
inline void close_on_exec(int fd) { call(nr::fcntl, fd, F_SETFD, FD_CLOEXEC); }

}  // namespace oks
