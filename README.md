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
layers = { "kernel-abi" = { name = "openkal-svr4", version = "0.3.0", features = ["standalone"] } }
```

## What 0.3 adds: programs that start programs

`openkal.process` and `openkal.space` are claimed. A program starts another (`fork`,
`execve`, `wait`, `kill`: `kal_process_spawn` and its kin), waits for it, ends it, gives it
streams, a null device or a channel (a pipe), and starts a context in a copy of its space
(`kal_space_start`, on `fork`), over which a C library composes `fork` itself. So a shell or
a command interpreter built for this row starts external commands. What is known of a
program (`kal_process_info`) is its identifier, whether it has ended and its processor time
(`times`); its start and its memory are not, and `kal_process_open_id` is not claimed. A
start's resource preference reaches the program in `KAL_RESOURCES`, which
`kal_env_resources` reads.

The source had been written for 0.1 and kept: under the emulation the replacement of an
image left it (below), so a started program could not be observed. It is run now on the
System V kernel and under ibcs-us 4.3-2, where it stays. Three things it lacked were found
by the suite and added (a handle to the program itself and what is known of a program, the
channel, the preference), and one was corrected: the kernel's `wait` answers the status in
its second result, and ibcs-us does so only when it is also given a place for it, as a C
library's `wait(&status)` gives; the call now gives one.

## What 0.2 changes (openkal 0.26)

Nothing that a program built over this package observes: the manifest declares
`openkal = "^0.26.0"`. What 0.26 requires is of `openkal.process`, which this package
does not claim: a start bounded by the environment alone (clause 7.6) and a unit claimed
only where a reference can be made (clause 7.18). The arguments and the variables a
program receives are read from the vectors the kernel gave it, where they lie, so no
table bounds their number or their length.

The source kept for `openkal.process` (`src/unclaimed/process.cpp`, not built) is held to
the same clauses for the day it is claimed: it allocates both vectors of a start at their
measured size, it answers `kal_err_not_supported` from `kal_process_job_create` and does
not state `KAL_PROCESS_PROP_JOB_OBSERVE`, and the kernel's refusal of a start for its
size (`E2BIG`) is now `kal_err_no_space` in the translation every source shares
(`src/common.h`), where it was `kal_err_io`. None of this had been run in 0.2; from 0.3 the
source is `src/process.cpp`, built, run and claimed (above).

**`kal_env_var_at` answers a variable's name.** 0.1 answered the whole entry,
`NAME=value`, which `kal_env_var` does not find again; `openkal/env.h` states the name.
The specification's suite reported the enumeration as not observed, because the first
entry already failed, so the defect was read as an empty environment.

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
`openkal.fs`, `openkal.terminal`, `openkal.process` and `openkal.space` (from 0.3).

| Absent | Reason |
|---|---|
| `openkal.task` | Release 4.0's kernel has no threads. A program that needs them is refused when it is planned |
| `openkal.random` | No random device; the C library's allocator is musl's own (`malloc-mallocng`) |
| `openkal.exec`, `timeout`, `map`, `region`, `net`, `datagram`, `prof`, `debug` | Not provided |

Stated limits: the working directory's name is the environment's (`PWD`) when that names the
same directory, and is otherwise found a level at a time, as the systems' own `getcwd` finds it;
a file's position is a word (a file ends before 2 GiB); the wall clock is
in seconds (`time`) and the monotonic clock in ticks (`times`, 10 ms); a directory handle
is a path, since no call takes a directory's descriptor and a name beneath it; a lock of
these systems belongs to the program and not to the open file, so `KAL_FS_PROP_LOCKS` is
not claimed; a transfer at an offset moves the position and restores it, which a program
of one context cannot observe.

## Where it was run

**On a System V kernel and under an emulation of the interface, both by CI from 0.3.**

The kernel is AT&T UNIX System V/386 Release 4.0 Version 3.0, installed in QEMU from its
floppy set (2026-11-19). The media are not free software and are neither held nor
distributed here. The run can be repeated through xim's `svr4-disk-image` (technical
verification only: the system installed by its own installer, with a root shell on its
first serial line) and dirun 0.8.0, which gives a run's files on a second disk behind the
system's own label:

    dirun --profile <svr4-disk-image>/dirun.toml --emulator qemu-system-x86_64 -- ./program

A project runs the row there by stating that runner for `i686-svr4-musl` (the package's
README shows the table); the platform's own runner stays the emulation below.

The emulation is [ibcs-us](https://ibcs-us.sourceforge.io/) 4.3 with corrections (below), on
Linux x86_64, as `unshare -Ur ibcs-us <program>`. Three of its interfaces were used:
UnixWare 7 (`per_uw7`, what an ELF with no flags is taken as), Solaris (`per_solaris`)
and OpenServer 5 (`per_osr5`). It has no interface of Release 4.0 itself.

| What | On Release 4.0 Version 3.0 | Under each of the emulation's three interfaces |
|---|---|---|
| The specification's suite, 0.3 (openkal 0.26.0, `tests/cts`; features `env,time,fs,terminal,process,space`) | 168 observations held, 0 did not (the terminal's are made: the line is a terminal) | 157 observations held, 0 did not (ibcs-us 4.3-2) |
| The specification's suite, 0.1 (openkal 0.25.3; features `env,time,fs,terminal`) | 111 observations held, 0 did not | 111 observations held, 0 did not |
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
for the calls these programs issue. Two more differences are known and handled: after
`fork` the system gives the second program a non-zero second result, and the emulation a
zero first result; and `wait` answers the status in its second result on the system, and
under the emulation only when it is given a place for it as well.

## The runner

ibcs-us 4.3 as released did not load an executable without debugging information, list a
directory, shorten a file or change a file's mode. The one used here is built from its
released source with seven corrections, each found by running this package's programs:
[mcpp-community/ibcs-us](https://github.com/mcpp-community/ibcs-us), release `4.3-2`
(the xlings package `ibcs-us`). The host must let an unprivileged user make a user
namespace.

Until 4.3-2 a program that replaced its image left the emulation: ibcs-us handed the call
to Linux, which started the file as its own program, where its first call through the
gate faulted; that is why 0.1 and 0.2 did not claim `openkal.process`. From 4.3-2 a program
of the interface that replaces its image is started under the emulation again.

## Checks

`tools/row.sh <dir> <openkal-musl> <openkal-llvm-runtime> <ibcs-us>` writes a platform
package that builds the row from working trees (`tests/row`), and
`tools/run-conformance.sh <openkal> <dir> <ibcs-us> [<interface>]` builds the
specification's suite with it, a feature for each interface the manifest claims, and runs
it; `tools/run-conformance.sh <openkal> <dir> --kernel <dirun> <svr4-disk-image>
<qemu-system-x86_64>` runs it on the System V kernel instead, from xim's `svr4-disk-image`
(technical verification only: the system is proprietary), in QEMU's translator.
`.github/workflows/ci.yml` does both: under the three interfaces and on the kernel, and runs
`examples/hello` through `mcpp run`.
