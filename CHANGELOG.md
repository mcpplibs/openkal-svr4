# Changelog

## 0.3.0

- `openkal.process` and `openkal.space` are claimed (`src/process.cpp`, built from now). A
  program starts, waits for, ends and gives streams to another, and starts a context in a
  copy of its space (`kal_space_start` on `fork`, with `KAL_SPACE_PROP_CLONE_HANDLES` and
  `KAL_SPACE_PROP_DEFERRED_COPY`: under ibcs-us the copy is Linux's, which completes it
  lazily). Measured with the specification's suite (openkal 0.26.0): on AT&T UNIX System V/386
  Release 4.0 Version 3.0, 168 observations held and 0 did not; under ibcs-us 4.3-2, 157 held
  and 0 did not under each of its three interfaces.
- Added where the suite found them missing: `kal_process_self` and `kal_process_info` (the
  identifier, whether the program ended, its processor time from `times`; for a started
  program, what the children's times grew by across the `wait` that collected it), a channel
  (`kal_process_channel`, a pipe; `KAL_PROCESS_PROP_CHANNEL`), the resource preference of a
  start (`KAL_RESOURCES`, read by `kal_env_resources`; two at once are `kal_err_invalid`),
  and `kal_process_end_as_requested` answering `kal_err_invalid` where no request was made.
- `wait` is given a place for the status: the kernel answers in the second result
  regardless, and ibcs-us only when it has one.
- `tools/run-conformance.sh` selects a feature of the suite for every interface the
  manifest claims, and runs on the System V kernel with `--kernel` (dirun and xim's
  svr4-disk-image, technical verification only). CI runs both, under ibcs-us 4.3-2.

## 0.2.0

- The specification is declared as `^0.26.0`. What openkal 0.26 requires is of
  `openkal.process` (clauses 7.6 and 7.18), which this package does not claim; nothing a
  program observes changes.
- `E2BIG` is `kal_err_no_space` in the shared translation (`src/common.h`), where it was
  `kal_err_io`: only `execve` answers it, which the sources built do not issue, and the
  source kept for `openkal.process` is thereby held to clause 7.6. Not run.
- `kal_env_var_at` answers a variable's name, as `openkal/env.h` states; 0.1 answered the
  whole entry (`NAME=value`), which `kal_env_var` does not find again.

## 0.1.0

- openkal for UNIX System V Release 4 and its descendants on i386, on the kernel's
  system-call interface: seven interfaces of openkal 0.25 (`abort`, `stream`, `memory`,
  `env`, `time`, `fs`, `terminal`), standalone. The part of the interface the systems
  share is the one issued, so one executable runs on each; a call a system lacks is
  replaced by the older one or reported as not supported.
- Run under an emulation of the interface (ibcs-us 4.3 with corrections; UnixWare 7,
  Solaris and OpenServer 5), where the specification's suite holds in every observation
  made. Not run on a System V kernel.
