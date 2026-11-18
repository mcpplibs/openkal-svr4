// openkal.fs on the calls the systems share: `open', `stat', `lstat', `fstat',
// `getdents', `mkdir', `rmdir', `unlink', `rename', `utime', `chmod', `fcntl'.
#include "common.h"

namespace {
using oks::word;

// THE TABLE OF DIRECTORY HANDLES (common.h): a path each. Slots 0 and 1 are the
// working directory and the root and are never released.
constexpr int kDirs = 48;
char g_dir[kDirs][oks::kPath];
bool g_used[kDirs];
char g_here[oks::kPath];            // the working directory's own name, for the grant
bool g_ready = false;

int kind_of(unsigned mode) {
    switch (mode & oks::S_IFMT) {
        case oks::S_IFREG: return kal_node_file;
        case oks::S_IFDIR: return kal_node_directory;
        case oks::S_IFLNK: return kal_node_link;
        default:           return kal_node_other;
    }
}
// A name's own node, a link not followed. A system of Release 3's calls has no
// `lstat' (measured under the OpenServer 5 interface) and no symbolic link
// either, so `stat' answers the same question there.
oks::outcome own(const char* path, oks::kstat* st) {
    const auto r = oks::call(oks::nr::lstat, oks::arg(path), oks::arg(st));
    return r.error == oks::ENOSYS ? oks::call(oks::nr::stat, oks::arg(path), oks::arg(st)) : r;
}
bool same_node(const oks::kstat& a, const oks::kstat& b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }

// The working directory's name. No call of these systems answers it. The name
// the environment gives (PWD) is taken when it names the same directory as `.':
// a program started by a shell has it. Otherwise the name is found as the
// systems' own `getcwd' finds it, a level at a time: the entry of the directory
// above that is the same node as the one below. Nodes are compared as `stat'
// answers them for both, so the comparison does not depend on how a listing
// states a node's number. Where a level cannot be read the name is `.', which
// every operation still resolves.
bool find_here(char* out, unsigned long cap) {
    char up[oks::kPath];                 // `.', `./..', `./../..', ...
    char found[oks::kPath];              // the name, assembled from its end
    unsigned long at = sizeof found - 1, len = 1;
    found[at] = 0;
    up[0] = '.'; up[1] = 0;
    for (int level = 0; level < 256; ++level) {
        oks::kstat below{}, above{};
        if (len + 3 + 256 >= sizeof up) return false;
        if (oks::call(oks::nr::stat, oks::arg(up), oks::arg(&below)).failed()) return false;
        oks::copy(up + len, "/..", 4); len += 3;
        if (oks::call(oks::nr::stat, oks::arg(up), oks::arg(&above)).failed()) return false;
        if (same_node(below, above)) {                       // the root is its own parent
            if (found[at] == 0) { if (at == 0) return false; found[--at] = '/'; }
            const unsigned long n = sizeof found - at;       // with the terminator
            if (n > cap) return false;
            oks::copy(out, found + at, n);
            return true;
        }
        const auto dir = oks::call(oks::nr::open, oks::arg(up), oks::O_RDONLY, 0);
        if (dir.failed()) return false;
        const int fd = static_cast<int>(dir.value);
        bool have = false;
        char buf[1024];
        while (!have) {
            const auto r = oks::call(oks::nr::getdents, fd, oks::arg(buf), sizeof buf);
            if (r.error == oks::EINTR) continue;
            if (r.failed() || r.value <= 0) break;
            for (long pos = 0; pos < r.value && !have;) {
                auto* e = reinterpret_cast<oks::kdirent*>(buf + pos);
                if (e->d_reclen == 0) break;
                pos += e->d_reclen;
                const unsigned long n = oks::length(e->d_name);
                if (n == 0 || n > 255) continue;
                if ((n == 1 && e->d_name[0] == '.') || (n == 2 && e->d_name[0] == '.' && e->d_name[1] == '.')) continue;
                up[len] = '/'; oks::copy(up + len + 1, e->d_name, n + 1);
                oks::kstat st{};
                const bool is = !own(up, &st).failed() && kind_of(st.st_mode) == kal_node_directory && same_node(st, below);
                up[len] = 0;
                if (!is) continue;
                if (at < n + 1) break;
                at -= n; oks::copy(found + at, e->d_name, n);
                found[--at] = '/';
                have = true;
            }
        }
        oks::call(oks::nr::close, fd);
        if (!have) return false;
    }
    return false;
}
void name_here() {
    g_here[0] = '.'; g_here[1] = 0;
    const char* pwd = oks::variable("PWD", 3);
    if (pwd && pwd[0] == '/' && oks::length(pwd) < sizeof g_here) {
        oks::kstat a{}, b{};
        if (!oks::call(oks::nr::stat, oks::arg("."), oks::arg(&a)).failed()
            && !oks::call(oks::nr::stat, oks::arg(pwd), oks::arg(&b)).failed() && same_node(a, b)) {
            oks::copy(g_here, pwd, oks::length(pwd) + 1);
            return;
        }
    }
    char name[oks::kPath];
    if (find_here(name, sizeof name)) oks::copy(g_here, name, oks::length(name) + 1);
}
void ready() {
    if (g_ready) return;
    g_dir[0][0] = '.'; g_dir[0][1] = 0; g_used[0] = true;
    g_dir[1][0] = '/'; g_dir[1][1] = 0; g_used[1] = true;
    name_here();
    g_ready = true;
}
int slot_of(kal_dir d) {
    ready();
    if (d.h == 0 || d.h > static_cast<kal_uintptr>(kDirs)) return -1;
    const int i = static_cast<int>(d.h - 1);
    return g_used[i] ? i : -1;
}

void fill_info(const oks::kstat& st, kal_node_info* out, kal_u32 mask = ~kal_u32{0}) {
    const kal_u32 cap = out->self_size;
    kal_node_info i{};
    i.self_size = cap;
    i.present = KAL_INFO_KIND | KAL_INFO_SIZE | KAL_INFO_MODIFIED | KAL_INFO_WRITABLE | KAL_INFO_IDENTITY;
    i.kind = kind_of(st.st_mode);
    i.size = static_cast<kal_u64>(static_cast<unsigned long>(st.st_size));
    i.modified_ns = static_cast<kal_u64>(static_cast<unsigned long>(st.st_mtime)) * 1000000000ull;
    i.identity[0] = static_cast<kal_u64>(static_cast<unsigned short>(st.st_dev));
    i.identity[1] = static_cast<kal_u64>(st.st_ino);
    i.writable = (st.st_mode & 0222) != 0;
    if (i.kind == kal_node_file) { i.present |= KAL_INFO_EXECUTABLE; i.executable = (st.st_mode & 0111) != 0; }
    i.present &= mask | KAL_INFO_KIND;
    // A CALLER'S STRUCTURE MAY PREDATE A FIELD: one that ends before a field
    // does is not told the field is present.
    static const struct { kal_u32 bit; unsigned long end; } fields[] = {
        { KAL_INFO_SIZE,       __builtin_offsetof(kal_node_info, size) + 8 },
        { KAL_INFO_MODIFIED,   __builtin_offsetof(kal_node_info, modified_ns) + 8 },
        { KAL_INFO_IDENTITY,   __builtin_offsetof(kal_node_info, identity) + 16 },
        { KAL_INFO_KIND,       __builtin_offsetof(kal_node_info, kind) + sizeof(int) },
        { KAL_INFO_WRITABLE,   __builtin_offsetof(kal_node_info, writable) + sizeof(int) },
        { KAL_INFO_EXECUTABLE, __builtin_offsetof(kal_node_info, executable) + sizeof(int) },
    };
    for (const auto& f : fields) if (cap < f.end) i.present &= ~f.bit;
    if (!(i.present & KAL_INFO_SIZE)) i.size = 0;
    if (!(i.present & KAL_INFO_MODIFIED)) i.modified_ns = 0;
    if (!(i.present & KAL_INFO_IDENTITY)) i.identity[0] = i.identity[1] = 0;
    if (!(i.present & KAL_INFO_WRITABLE)) i.writable = 0;
    if (!(i.present & KAL_INFO_EXECUTABLE)) i.executable = 0;
    oks::copy(out, &i, cap < sizeof i ? cap : sizeof i);
}

// THE NAME AN OPEN FILE WAS OPENED BY. A modification time is set by a name on
// these systems (`utime'), so an open file keeps the one it was opened with.
struct opened { int fd; char* path; unsigned long size; };
constexpr int kOpened = 64;
opened g_opened[kOpened];
void remember(int fd, const char* path) {
    for (auto& o : g_opened) {
        if (o.path) continue;
        const unsigned long n = oks::length(path) + 1;
        o.path = static_cast<char*>(kal_alloc(n, 1));
        if (!o.path) return;
        oks::copy(o.path, path, n); o.fd = fd; o.size = n;
        return;
    }
}
const char* name_of(int fd) {
    for (auto& o : g_opened) if (o.path && o.fd == fd) return o.path;
    return nullptr;
}
void forget(int fd) {
    for (auto& o : g_opened) if (o.path && o.fd == fd) { kal_free(o.path, o.size, 1); o.path = nullptr; }
}

// A LISTING IN PROGRESS: its own descriptor of the directory, its path for the
// kind of each entry, and the entries `getdents' last read.
struct lister {
    int fd;
    long pos, end;
    char path[oks::kPath];
    alignas(4) char buf[4096];
};

int next_entry(kal_uintptr* iter, char* name, kal_uintptr cap, kal_uintptr* nlen, int* kind, oks::kstat* st) {
    if (!iter) return kal_err_invalid;
    auto* l = reinterpret_cast<lister*>(*iter);
    if (!l) { if (nlen) *nlen = 0; return kal_ok; }
    for (;;) {
        if (l->pos >= l->end) {
            const auto r = oks::call(oks::nr::getdents, l->fd, oks::arg(l->buf), sizeof l->buf);
            if (r.error == oks::EINTR) continue;
            if (r.failed() || r.value <= 0) {
                oks::call(oks::nr::close, l->fd);
                kal_free(l, sizeof *l, alignof(lister));
                *iter = 0;
                if (nlen) *nlen = 0;
                return r.failed() ? oks::translate(r.error) : kal_ok;
            }
            l->pos = 0; l->end = r.value;
        }
        auto* e = reinterpret_cast<oks::kdirent*>(l->buf + l->pos);
        if (e->d_reclen == 0) { l->pos = l->end; continue; }
        l->pos += e->d_reclen;
        const unsigned long n = oks::length(e->d_name);
        if ((n == 1 && e->d_name[0] == '.') || (n == 2 && e->d_name[0] == '.' && e->d_name[1] == '.')) continue;
        oks::put(e->d_name, n, name, cap);
        if (nlen) *nlen = n;
        oks::kstat s{};
        bool have = false;
        const unsigned long base = oks::length(l->path);
        if (base + 1 + n < sizeof l->path) {
            char full[oks::kPath];
            oks::copy(full, l->path, base); full[base] = '/'; oks::copy(full + base + 1, e->d_name, n + 1);
            have = !own(full, &s).failed();
        }
        if (kind) *kind = have ? kind_of(s.st_mode) : kal_node_other;
        if (st) { if (have) *st = s; else oks::zero(st, sizeof *st); }
        return kal_ok;
    }
}

int absent(kal_node_info* out) {
    const kal_u32 cap = out->self_size;
    kal_node_info i{}; i.self_size = cap;
    i.present = KAL_INFO_KIND; i.kind = kal_node_absent;
    oks::copy(out, &i, cap < sizeof i ? cap : sizeof i);
    return kal_ok;
}

}  // namespace

namespace oks {
const char* dir_path(kal_dir d) { const int i = slot_of(d); return i < 0 ? nullptr : g_dir[i]; }
bool join(kal_dir base, const char* name, kal_uintptr len, char* out) {
    const char* b = dir_path(base);
    if (!b || !name || len == 0 || name[0] == '/') return false;
    for (kal_uintptr i = 0; i < len; ) {
        kal_uintptr j = i;
        while (j < len && name[j] != '/') { if (name[j] == 0) return false; ++j; }
        if (j - i == 2 && name[i] == '.' && name[i + 1] == '.') return false;
        i = j + 1;
    }
    const unsigned long bn = length(b);
    const bool slash = bn > 0 && b[bn - 1] != '/';
    if (bn + (slash ? 1 : 0) + len >= kPath) return false;
    copy(out, b, bn);
    unsigned long at = bn;
    if (slash) out[at++] = '/';
    copy(out + at, name, len);
    out[at + len] = 0;
    return true;
}
}  // namespace oks

extern "C" {

kal_uintptr kal_fs_preopen_count(void) { ready(); return 2; }
int kal_fs_preopen(kal_uintptr i, kal_dir* out, char* name, kal_uintptr cap, kal_uintptr* len) {
    ready();
    if (i >= 2 || !out) return kal_err_invalid;
    *out = kal_dir{ i + 1 };
    const char* n = i == 0 ? g_here : "/";
    oks::put(n, oks::length(n), name, cap);
    if (len) *len = oks::length(n);
    return kal_ok;
}
// What is true on every one of these systems: names are compared by their
// bytes, a file has a modification time, links exist and are made, a rename
// replaces its target in one step, and an open file can be removed.
kal_uintptr kal_fs_props(kal_dir) {
    return KAL_FS_PROP_CASE_SENSITIVE | KAL_FS_PROP_LINKS | KAL_FS_PROP_MODIFIED_TIME
         | KAL_FS_PROP_ATOMIC_RENAME | KAL_FS_PROP_EXECUTABLE
         | KAL_FS_PROP_REMOVE_OPEN | KAL_FS_PROP_HARD_LINKS;
}
kal_uintptr kal_fs_max_name(void) { return oks::kPath - 1; }

int kal_fs_open_dir(kal_dir base, const char* name, kal_uintptr len, kal_dir* out) {
    char p[oks::kPath];
    if (!out || !oks::join(base, name, len, p)) return kal_err_invalid;
    oks::kstat st{};
    const auto r = oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st));
    if (r.failed()) return oks::translate(r.error);
    if (kind_of(st.st_mode) != kal_node_directory) return kal_err_not_directory;
    for (int i = 2; i < kDirs; ++i) {
        if (g_used[i]) continue;
        g_used[i] = true;
        oks::copy(g_dir[i], p, oks::length(p) + 1);
        *out = kal_dir{ static_cast<kal_uintptr>(i) + 1u };
        return kal_ok;
    }
    return kal_err_no_memory;
}
void kal_fs_close_dir(kal_dir d) {
    const int i = slot_of(d);
    if (i >= 2) g_used[i] = false;
}

int kal_fs_open(kal_dir base, const char* name, kal_uintptr len, kal_uintptr f, kal_file* out) {
    char p[oks::kPath];
    if (!out || !oks::join(base, name, len, p)) return kal_err_invalid;
    if (!(f & (KAL_OPEN_READ | KAL_OPEN_WRITE))) return kal_err_invalid;
    int flags = (f & KAL_OPEN_READ) && (f & KAL_OPEN_WRITE) ? oks::O_RDWR
              : (f & KAL_OPEN_WRITE) ? oks::O_WRONLY : oks::O_RDONLY;
    if (f & KAL_OPEN_CREATE)    flags |= oks::O_CREAT;
    if (f & KAL_OPEN_EXCLUSIVE) flags |= oks::O_EXCL;
    if (f & KAL_OPEN_TRUNCATE)  flags |= oks::O_TRUNC;
    if (f & KAL_OPEN_APPEND)    flags |= oks::O_APPEND;
    // A directory opens for reading on these systems; it is not a file here.
    oks::kstat st{};
    if (!oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st)).failed() && kind_of(st.st_mode) == kal_node_directory)
        return kal_err_is_directory;
    const auto r = oks::call(oks::nr::open, oks::arg(p), flags, 0666);
    if (r.failed()) return oks::translate(r.error);
    oks::close_on_exec(static_cast<int>(r.value));
    remember(static_cast<int>(r.value), p);
    *out = kal_file{ oks::pack(static_cast<int>(r.value)) };
    return kal_ok;
}
void kal_fs_close_file(kal_file f) {
    if (!f.h) return;
    forget(oks::unpack(f.h));
    oks::call(oks::nr::close, oks::unpack(f.h));
}
kal_stream kal_fs_stream(kal_file f) { return kal_stream{ f.h }; }

int kal_fs_seek(kal_file f, kal_i64 offset, int whence, kal_u64* result) {
    const int fd = oks::unpack(f.h);
    if (fd < 0 || whence < 0 || whence > 2) return kal_err_invalid;
    // A position is a word on these systems: a file ends before 2 GiB.
    if (offset > 0x7fffffffll || offset < -0x7fffffffll - 1) return kal_err_invalid;
    const auto r = oks::call(oks::nr::lseek, fd, static_cast<word>(offset), whence);
    if (r.failed()) return oks::translate(r.error);
    if (result) *result = static_cast<kal_u64>(static_cast<unsigned long>(r.value));
    return kal_ok;
}
int kal_fs_truncate(kal_file f, kal_u64 size) {
    const int fd = oks::unpack(f.h);
    if (fd < 0 || size > 0x7fffffffull) return kal_err_invalid;
    oks::kstat st{};
    const auto s = oks::call(oks::nr::fstat, fd, oks::arg(&st));
    if (s.failed()) return oks::translate(s.error);
    if (static_cast<kal_u64>(static_cast<unsigned long>(st.st_size)) > size) {
        // Release 4 frees a file's space from an offset to its end (F_FREESP).
        oks::kflock l{}; l.l_whence = 0; l.l_start = static_cast<oks::i32>(size); l.l_len = 0;
        const auto r = oks::call(oks::nr::fcntl, fd, oks::F_FREESP, oks::arg(&l));
        // WHAT WAS ASKED IS READ BACK. A host that emulates the interface may
        // accept the command and free nothing (measured); a later system's own
        // call is then taken (`ftruncate'), and where neither shortens the file
        // the operation reports that it cannot, never that it did.
        oks::kstat now{};
        if (!r.failed() && !oks::call(oks::nr::fstat, fd, oks::arg(&now)).failed()
            && static_cast<kal_u64>(static_cast<unsigned long>(now.st_size)) == size) return kal_ok;
        const auto t = oks::call(oks::nr::ftruncate, fd, static_cast<word>(size));
        if (!t.failed()) return kal_ok;
        return r.failed() && r.error != oks::EINVAL ? oks::translate(r.error) : kal_err_not_supported;
    }
    if (static_cast<kal_u64>(static_cast<unsigned long>(st.st_size)) < size) {
        // Longer: a byte written at the last position, and the position restored.
        const auto here = oks::call(oks::nr::lseek, fd, 0, 1);
        if (here.failed()) return oks::translate(here.error);
        const auto to = oks::call(oks::nr::lseek, fd, static_cast<word>(size - 1), 0);
        if (to.failed()) return oks::translate(to.error);
        const char z = 0;
        const auto w = oks::call(oks::nr::write, fd, oks::arg(&z), 1);
        oks::call(oks::nr::lseek, fd, here.value, 0);
        if (w.failed()) return oks::translate(w.error);
    }
    return kal_ok;
}
// A TRANSFER AT AN OFFSET IS THE POSITION MOVED AND RESTORED. No call of the
// shared part transfers at an offset (`pread' is a later system's). A program
// of these rows has one context (openkal.task is absent), so nothing observes
// the position between the two moves.
namespace {
kal_intptr at_offset(kal_file f, void* buf, kal_uintptr len, kal_u64 offset, bool write) {
    const int fd = oks::unpack(f.h);
    if (fd < 0 || offset > 0x7fffffffull) return -kal_err_invalid;
    const auto here = oks::call(oks::nr::lseek, fd, 0, 1);
    if (here.failed()) return -static_cast<kal_intptr>(oks::translate(here.error));
    const auto to = oks::call(oks::nr::lseek, fd, static_cast<word>(offset), 0);
    if (to.failed()) return -static_cast<kal_intptr>(oks::translate(to.error));
    kal_uintptr done = 0; int why = 0;
    while (done < len) {
        const auto r = oks::call(write ? oks::nr::write : oks::nr::read, fd,
                                 oks::arg(static_cast<char*>(buf) + done), static_cast<word>(len - done));
        if (r.error == oks::EINTR) continue;
        if (r.failed()) { why = oks::translate(r.error); break; }
        if (r.value == 0) break;
        done += static_cast<kal_uintptr>(r.value);
        if (!write) break;                      // a read answers what one read gave
    }
    oks::call(oks::nr::lseek, fd, here.value, 0);
    return done == 0 && why ? -static_cast<kal_intptr>(why) : static_cast<kal_intptr>(done);
}
}  // namespace
kal_intptr kal_fs_read_at(kal_file f, void* buf, kal_uintptr len, kal_u64 offset) {
    return at_offset(f, buf, len, offset, false);
}
kal_intptr kal_fs_write_at(kal_file f, const void* buf, kal_uintptr len, kal_u64 offset) {
    return at_offset(f, const_cast<void*>(buf), len, offset, true);
}

int kal_fs_info(kal_dir base, const char* name, kal_uintptr len, kal_uintptr flags, kal_u32, kal_node_info* out) {
    char p[oks::kPath];
    if (!out || !oks::join(base, name, len, p)) return kal_err_invalid;
    oks::kstat st{};
    const auto r = (flags & KAL_FS_NO_RESOLVE) ? own(p, &st) : oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st));
    if (r.failed()) return r.error == oks::ENOENT ? absent(out) : oks::translate(r.error);
    fill_info(st, out);
    return kal_ok;
}
int kal_fs_file_info(kal_file f, kal_u32, kal_node_info* out) {
    if (!out) return kal_err_invalid;
    oks::kstat st{};
    const auto r = oks::call(oks::nr::fstat, oks::unpack(f.h), oks::arg(&st));
    if (r.failed()) return oks::translate(r.error);
    fill_info(st, out);
    return kal_ok;
}

int kal_fs_mkdir(kal_dir base, const char* name, kal_uintptr len) {
    char p[oks::kPath];
    if (!oks::join(base, name, len, p)) return kal_err_invalid;
    const auto r = oks::call(oks::nr::mkdir, oks::arg(p), 0777);
    return r.failed() ? oks::translate(r.error) : kal_ok;
}
int kal_fs_remove(kal_dir base, const char* name, kal_uintptr len) {
    char p[oks::kPath];
    if (!oks::join(base, name, len, p)) return kal_err_invalid;
    oks::kstat st{};
    const auto s = own(p, &st);
    if (s.failed()) return oks::translate(s.error);
    if (kind_of(st.st_mode) == kal_node_directory) {
        const auto r = oks::call(oks::nr::rmdir, oks::arg(p));
        // Release 4 reports a directory that is not empty as one that exists.
        if (r.error == oks::EEXIST) return kal_err_not_empty;
        return r.failed() ? oks::translate(r.error) : kal_ok;
    }
    const auto r = oks::call(oks::nr::unlink, oks::arg(p));
    return r.failed() ? oks::translate(r.error) : kal_ok;
}
int kal_fs_rename(kal_dir from, const char* a, kal_uintptr alen, kal_dir to, const char* b, kal_uintptr blen) {
    char pa[oks::kPath], pb[oks::kPath];
    if (!oks::join(from, a, alen, pa) || !oks::join(to, b, blen, pb)) return kal_err_invalid;
    const auto r = oks::call(oks::nr::rename, oks::arg(pa), oks::arg(pb));
    if (r.error == oks::EEXIST) return kal_err_not_empty;
    if (r.error != oks::ENOSYS) return r.failed() ? oks::translate(r.error) : kal_ok;
    // A SYSTEM OF RELEASE 3's CALLS HAS NO `rename' AT THIS NUMBER (measured
    // under the OpenServer 5 interface: "Function not implemented"). A file is
    // renamed there as that release's own `mv' did it: the target's name is
    // removed, the file is given it, and loses its first. The three steps are
    // not one, which the property of an atomic rename states for Release 4 and
    // which no call of the older interface can provide; a directory has no
    // second name to be given, and is not renamed.
    oks::kstat st{};
    const auto s = own(pa, &st);
    if (s.failed()) return oks::translate(s.error);
    if (kind_of(st.st_mode) == kal_node_directory) return kal_err_not_supported;
    const auto gone = oks::call(oks::nr::unlink, oks::arg(pb));
    if (gone.failed() && gone.error != oks::ENOENT) return oks::translate(gone.error);
    const auto l = oks::call(oks::nr::link, oks::arg(pa), oks::arg(pb));
    if (l.failed()) return oks::translate(l.error);
    const auto u = oks::call(oks::nr::unlink, oks::arg(pa));
    return u.failed() ? oks::translate(u.error) : kal_ok;
}
int kal_fs_rename_flags(kal_dir from, const char* a, kal_uintptr alen, kal_dir to, const char* b, kal_uintptr blen,
                        kal_uintptr flags) {
    if (flags == 0) return kal_fs_rename(from, a, alen, to, b, blen);
    if (flags != KAL_RENAME_NO_REPLACE) return kal_err_invalid;
    // A file is given its second name only where none exists (`link'), and then
    // loses its first: the one step these systems have that does not replace.
    char pa[oks::kPath], pb[oks::kPath];
    if (!oks::join(from, a, alen, pa) || !oks::join(to, b, blen, pb)) return kal_err_invalid;
    oks::kstat st{};
    const auto s = own(pa, &st);
    if (s.failed()) return oks::translate(s.error);
    if (kind_of(st.st_mode) == kal_node_directory) return kal_err_not_supported;
    const auto l = oks::call(oks::nr::link, oks::arg(pa), oks::arg(pb));
    if (l.failed()) return oks::translate(l.error);
    const auto u = oks::call(oks::nr::unlink, oks::arg(pa));
    return u.failed() ? oks::translate(u.error) : kal_ok;
}
int kal_fs_hard_link(kal_dir from, const char* a, kal_uintptr alen, kal_dir to, const char* b, kal_uintptr blen) {
    char pa[oks::kPath], pb[oks::kPath];
    if (!oks::join(from, a, alen, pa) || !oks::join(to, b, blen, pb)) return kal_err_invalid;
    oks::kstat st{};
    const auto s = own(pa, &st);
    if (s.failed()) return oks::translate(s.error);
    if (kind_of(st.st_mode) == kal_node_directory) return kal_err_is_directory;
    const auto r = oks::call(oks::nr::link, oks::arg(pa), oks::arg(pb));
    return r.failed() ? oks::translate(r.error) : kal_ok;
}
kal_intptr kal_fs_link_read(kal_dir base, const char* name, kal_uintptr len, char* out, kal_uintptr cap) {
    char p[oks::kPath], v[oks::kPath];
    if (!oks::join(base, name, len, p)) return -kal_err_invalid;
    const auto r = oks::call(oks::nr::readlink, oks::arg(p), oks::arg(v), sizeof v);
    if (r.failed()) return -static_cast<kal_intptr>(oks::translate(r.error));
    return oks::put(v, static_cast<kal_uintptr>(r.value), out, cap);
}
int kal_fs_link_create(kal_dir, const char*, kal_uintptr, const char*, kal_uintptr, kal_uintptr) { return kal_err_not_supported; }
kal_intptr kal_fs_resolved_name(kal_dir, const char*, kal_uintptr, char*, kal_uintptr) { return -kal_err_not_supported; }

// A modification time is set by a name (`utime', in seconds): an open file's
// is set by the name it was opened with.
namespace {
int set_time(const char* p, kal_u64 ns) {
    oks::kstat st{};
    const auto s = oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st));
    if (s.failed()) return oks::translate(s.error);
    oks::kutimbuf t{ st.st_atime, static_cast<oks::i32>(ns / 1000000000ull) };
    const auto r = oks::call(oks::nr::utime, oks::arg(p), oks::arg(&t));
    return r.failed() ? oks::translate(r.error) : kal_ok;
}
}  // namespace
int kal_fs_set_modified(kal_file f, kal_u64 ns) {
    const char* p = name_of(oks::unpack(f.h));
    return p ? set_time(p, ns) : kal_err_not_supported;
}
int kal_fs_set_modified_at(kal_dir base, const char* name, kal_uintptr len, kal_u64 ns) {
    char p[oks::kPath];
    if (!oks::join(base, name, len, p)) return kal_err_invalid;
    return set_time(p, ns);
}
int kal_fs_set_executable_at(kal_dir base, const char* name, kal_uintptr len, int executable) {
    char p[oks::kPath];
    if (!oks::join(base, name, len, p)) return kal_err_invalid;
    oks::kstat st{};
    const auto s = oks::call(oks::nr::stat, oks::arg(p), oks::arg(&st));
    if (s.failed()) return oks::translate(s.error);
    if (kind_of(st.st_mode) == kal_node_directory) return kal_err_is_directory;
    if (kind_of(st.st_mode) != kal_node_file) return kal_err_invalid;
    // Execution for each class that may read, as a program's installer sets it.
    const unsigned mode = st.st_mode & 07777u;
    const unsigned next = executable ? (mode | ((mode & 0444u) >> 2)) : (mode & ~0111u);
    const auto r = oks::call(oks::nr::chmod, oks::arg(p), static_cast<word>(next));
    return r.failed() ? oks::translate(r.error) : kal_ok;
}

// A LOCK OF THESE SYSTEMS BELONGS TO THE PROGRAM, NOT TO THE OPEN FILE (`fcntl'
// record locks): a second open file of the same program is granted what the
// first holds, which is not the lock openkal states. The property is not
// claimed and the operations report so (measured: the suite's observation of a
// second open file did not hold over F_SETLK).
int kal_fs_lock(kal_file, kal_u64, kal_u64, kal_uintptr) { return kal_err_not_supported; }
int kal_fs_unlock(kal_file, kal_u64, kal_u64) { return kal_err_not_supported; }
int kal_fs_capacity(kal_dir, kal_u64*, kal_u64*) { return kal_err_not_supported; }

int kal_fs_list_begin(kal_dir d, kal_uintptr* iter) {
    const char* p = oks::dir_path(d);
    if (!p || !iter) return kal_err_invalid;
    const auto own = oks::call(oks::nr::open, oks::arg(p), oks::O_RDONLY, 0);
    if (own.failed()) return oks::translate(own.error);
    auto* l = static_cast<lister*>(kal_alloc(sizeof(lister), alignof(lister)));
    if (!l) { oks::call(oks::nr::close, own.value); return kal_err_no_memory; }
    oks::close_on_exec(static_cast<int>(own.value));
    l->fd = static_cast<int>(own.value); l->pos = 0; l->end = 0;
    oks::copy(l->path, p, oks::length(p) + 1);
    *iter = reinterpret_cast<kal_uintptr>(l);
    return kal_ok;
}
int kal_fs_list_next(kal_dir d, kal_uintptr* iter, char* name, kal_uintptr cap, kal_uintptr* nlen, int* kind) {
    if (!oks::dir_path(d)) return kal_err_invalid;
    return next_entry(iter, name, cap, nlen, kind, nullptr);
}
int kal_fs_list_next_info(kal_dir d, kal_uintptr* iter, char* name, kal_uintptr cap, kal_uintptr* nlen, kal_u32 wanted,
                          kal_node_info* info) {
    if (!oks::dir_path(d) || !info) return kal_err_invalid;
    oks::kstat st{}; int kind = 0;
    const int r = next_entry(iter, name, cap, nlen, &kind, &st);
    if (r == kal_ok && *iter) fill_info(st, info, wanted);
    return r;
}
int kal_fs_dir_attenuate(kal_dir, const kal_dir_rights*, kal_dir*) { return kal_err_not_supported; }

}  // extern "C"
