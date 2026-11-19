// The system call interface of System V on i386, as this implementation issues it.
//
// EVERY OPERATION BELOW IS A SYSTEM CALL, issued here; none is a function of a
// C library, and no header of a system is read: the systems' own headers are
// not free to copy, and a program of this implementation links none of their
// files.
//
// WHAT IS STATED HERE IS THE PART THE SYSTEMS SHARE. The entry (src/call.S),
// the numbers below and the layouts of `stat', `dirent', `termio' and `flock'
// are those of the Intel386 binary compatibility specification (iBCS2) and the
// System V ABI's Intel386 supplement, which UNIX System V Release 4 and its
// descendants on the architecture kept: one program runs on each. Sources: the
// two specifications, and NetBSD's `compat_svr4' (BSD licence) for the calls
// Release 4 added (mmap, poll, lstat, rename, getdents). Each was then run:
// see README.md for where.
//
// A call a system lacks answers ENOSYS (or its own error), and the operation
// above it reports `not supported' or takes the older call.
#pragma once

namespace oks {

using word = long;
using u8 = unsigned char; using u16 = unsigned short; using u32 = unsigned long;
using i16 = short; using i32 = long;

extern "C" long oks_call(long nr, long a, long b, long c, long d, long e, long f, long* second, long* error);

// A call's outcome: the result, and the error number (zero: none).
struct outcome { word value; word error; word second; bool failed() const { return error != 0; } };
inline outcome call(word nr, word a = 0, word b = 0, word c = 0, word d = 0, word e = 0, word f = 0) {
    outcome o{};
    o.value = oks_call(nr, a, b, c, d, e, f, &o.second, &o.error);
    return o;
}
template <class T> inline word arg(T* p) { return reinterpret_cast<word>(p); }

namespace nr {
constexpr word exit = 1, fork = 2, read = 3, write = 4, open = 5, close = 6, wait = 7, link = 9,
               unlink = 10, chdir = 12, time = 13, chmod = 15, brk = 17, stat = 18, lseek = 19,
               getpid = 20, fstat = 28, utime = 30, access = 33, kill = 37, pgrpsys = 39, dup = 41,
               pipe = 42, times = 43, signal = 48, sysi86 = 50, ioctl = 54, execve = 59, fcntl = 62,
               rmdir = 79, mkdir = 80, getdents = 81, poll = 87, lstat = 88, readlink = 90,
               mmap = 115, munmap = 117, rename = 134, uname = 135,
               ftruncate = 192;                 // UnixWare 7's; Release 4.0 frees space by `fcntl'
}

namespace abi {
constexpr int EPERM = 1, ENOENT = 2, ESRCH = 3, EINTR = 4, E2BIG = 7, ENOEXEC = 8, EBADF = 9, ECHILD = 10, EAGAIN = 11,
              ENOMEM = 12, EACCES = 13, EFAULT = 14, EEXIST = 17, ENOTDIR = 20, EISDIR = 21, EINVAL = 22,
              ENOTTY = 25, ENOSPC = 28, EROFS = 30, EPIPE = 32, ENAMETOOLONG = 78, ENOSYS = 89, ELOOP = 90,
              ENOTEMPTY = 93, EOPNOTSUPP = 122, ECONNRESET = 130;
constexpr int O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2, O_APPEND = 010, O_CREAT = 0400, O_TRUNC = 01000,
              O_EXCL = 02000;
constexpr int F_DUPFD = 0, F_GETFD = 1, F_SETFD = 2, F_SETLK = 6, F_SETLKW = 7, F_FREESP = 11;
constexpr int FD_CLOEXEC = 1;
constexpr int F_RDLCK = 1, F_WRLCK = 2, F_UNLCK = 3;
constexpr unsigned S_IFMT = 0170000, S_IFDIR = 0040000, S_IFREG = 0100000, S_IFLNK = 0120000;
constexpr int PROT_READ = 1, PROT_WRITE = 2, MAP_PRIVATE = 2;
constexpr int SIGINT = 2, SIGKILL = 9, SIGPIPE = 13, SIGABRT = 6;
constexpr word SIG_DFL = 0, SIG_IGN = 1;
constexpr word TCGETA = 0x5401, TCSETAW = 0x5403, TIOCGWINSZ = 0x5468;
constexpr int ISIG = 01, ICANON = 02, ECHO = 010;
constexpr int ISTRIP = 040, INLCR = 0100, IGNCR = 0200, ICRNL = 0400, IXON = 02000;
constexpr int VMIN = 4, VTIME = 5;
constexpr int HZ = 100;                       // the clock's ticks in a second, as `times' counts them
}  // namespace abi
using namespace abi;

// `stat' as the systems share it: the layout of Release 3, which Release 4
// keeps for the calls of these numbers (its own larger one is `xstat').
struct kstat {
    i16 st_dev; u16 st_ino; u16 st_mode; i16 st_nlink; u16 st_uid; u16 st_gid; i16 st_rdev; i16 pad_;
    i32 st_size; i32 st_atime; i32 st_mtime; i32 st_ctime;
};
static_assert(sizeof(kstat) == 32);
struct kdirent { i32 d_ino; i32 d_off; u16 d_reclen; char d_name[1]; };
struct ktermio { u16 c_iflag, c_oflag, c_cflag, c_lflag; char c_line; u8 c_cc[8]; };
static_assert(sizeof(ktermio) == 18);
struct kwinsize { u16 ws_row, ws_col, ws_xpixel, ws_ypixel; };
struct kflock { i16 l_type; i16 l_whence; i32 l_start; i32 l_len; i32 l_sysid; i32 l_pid; i32 pad_[4]; };
struct kutimbuf { i32 actime; i32 modtime; };
struct ktms { i32 utime, stime, cutime, cstime; };
struct kpollfd { int fd; i16 events; i16 revents; };

}  // namespace oks
