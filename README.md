# openkal-svr4

An implementation of [openkal](https://github.com/mcpplibs/openkal) for UNIX System V
Release 4 and its descendants on i386, written on the kernel's own system-call interface.

A program built over it links no file of the system: neither its C library nor its
headers. The C library is [openkal-musl](https://github.com/mcpplibs/openkal-musl), and
the program is one static ELF32 executable. The rows are a platform's
(`i686-svr4-musl`, openkal-platform), which states this package as the `kernel-abi` layer:

```toml
[[platform.provider]]
serves = ["i686-svr4-musl"]
layers = { "kernel-abi" = { name = "openkal-svr4", version = "0.1.0", features = ["standalone"] } }
```

## What it is written on

The part the systems share. The entry is the call gate (`lcall $7,$0`, `src/call.S`):
the number in `%eax`, the arguments on the stack beneath one slot, a failure by the carry
flag. The numbers and the layouts of `stat`, `dirent`, `termio` and `flock` (`src/sys.h`)
are those of the Intel386 binary compatibility specification (iBCS2) and the System V
ABI's Intel386 supplement; the calls Release 4 added (`mmap`, `poll`, `lstat`, `rename`,
`getdents`) are read from NetBSD's `compat_svr4` (BSD licence). No header of a system is
read or copied.

One executable therefore runs on each system that keeps that part. Where a system lacks
a call, the operation takes the older one or reports `not supported`:

| Call | Where absent | What is done |
|---|---|---|
| `rename` | Release 3's interface (OpenServer 5) | the target's name is removed, the file is given it and loses its first; a directory is not renamed |
| `lstat` | Release 3's interface | `stat`: such a system has no symbolic link |
| space freed by `fcntl` (`F_FREESP`) | where it frees nothing | what was asked is read back; `ftruncate` is taken, else `not supported` |

## Interfaces

`openkal.abort`, `openkal.stream`, `openkal.memory`, `openkal.env`, `openkal.time`,
`openkal.fs` and `openkal.terminal`.

| Absent | Reason |
|---|---|
| `openkal.task` | Release 4.0's kernel has no threads. A program that needs them is refused when it is planned |
| `openkal.random` | No random device; the C library's allocator is musl's own (`malloc-mallocng`) |
| `openkal.process` | Written (`src/unclaimed/process.cpp`) and not claimed: where it was run, the replacement of an image leaves the emulation, so the suite's observations of a started program cannot be made |
| `openkal.exec`, `timeout`, `map`, `region`, `space`, `net`, `datagram`, `prof`, `debug` | Outside 0.1 |

Stated limits: the working directory's name is the environment's (`PWD`) when that names the
same directory, and is otherwise found a level at a time, as the systems' own `getcwd` finds it;
a file's position is a word (a file ends before 2 GiB); the wall clock is
in seconds (`time`) and the monotonic clock in ticks (`times`, 10 ms); a directory handle
is a path, since no call takes a directory's descriptor and a name beneath it; a lock of
these systems belongs to the program and not to the open file, so `KAL_FS_PROP_LOCKS` is
not claimed; a transfer at an offset moves the position and restores it, which a program
of one context cannot observe.

## Where it was run

**On a System V kernel, once, by hand; and under an emulation of the interface, by CI.**

The kernel is AT&T UNIX System V/386 Release 4.0 Version 3.0, installed in QEMU from its
floppy set on the developer's machine (2026-11-19). The media are not free software and
are neither held nor distributed here. The installation and the runs are scripted: QEMU
driven through its monitor, the programs delivered as a tar archive on floppies, their
output on the first serial line.

The emulation is [ibcs-us](https://ibcs-us.sourceforge.io/) 4.3 with corrections (below), on
Linux x86_64, as `unshare -Ur ibcs-us <program>`. Three of its interfaces were used:
UnixWare 7 (`per_uw7`, what an ELF with no flags is taken as), Solaris (`per_solaris`)
and OpenServer 5 (`per_osr5`). It has no interface of Release 4.0 itself.

| What | On Release 4.0 Version 3.0 | Under each of the emulation's three interfaces |
|---|---|---|
| The specification's suite (openkal 0.25.3, `tests/cts`; features `env,time,fs,terminal`) | 111 observations held, 0 did not | 111 observations held, 0 did not |
| A C program; a C++23 program with `import std`, exceptions and `std::format` | run | run |
| `mcpp test` | not run there | run |
| An interpreter of 74 C sources and its batch script (files made, copied, renamed, listed, removed) | the script's output as on Linux x86_64: dev and release builds, LF and CR LF | the same |

One difference between the kernel and the emulation was found by the run and is the row's
since openkal-platform 0.30.1: the kernel kills a static executable that has a `PT_PHDR`
entry and no `PT_INTERP` one, which a link writes by default, so the row's link states the
program headers (`tests/row/svr4.ld` here). The emulation accepts either. On the kernel the
unoptimised executables were stripped of their debugging sections, to fit a floppy.

What the emulation shows is that a program issues the calls as the interface states them
and gets the answers it expects; what the kernel showed is that those are its answers too,
for the calls these programs issue. Another difference is known and handled: after `fork` the
system gives the second program a non-zero second result, and the emulation a zero first
result. `openkal.process` has been run on neither (below).

## The runner

ibcs-us 4.3 as released did not load an executable without debugging information, list a
directory, shorten a file or change a file's mode. The one used here is built from its
released source with seven corrections, each found by running this package's programs:
[mcpp-community/ibcs-us](https://github.com/mcpp-community/ibcs-us), release `4.3-1`
(the xlings package `ibcs-us`). The host must let an unprivileged user make a user
namespace.

A program that replaces its image leaves the emulation, which hands the call to Linux:
that is why `openkal.process` is not claimed.

## Checks

`tools/row.sh <dir> <openkal-musl> <openkal-llvm-runtime> <ibcs-us>` writes a platform
package that builds the row from working trees (`tests/row`), and
`tools/run-conformance.sh <openkal> <dir> <ibcs-us> [<interface>]` builds the
specification's suite with it and runs it. `.github/workflows/ci.yml` does both under the
three interfaces, and runs `examples/hello` through `mcpp run`.
