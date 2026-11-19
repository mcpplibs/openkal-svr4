# Changelog

## 0.2.0

- The specification is declared as `^0.26.0`. What openkal 0.26 requires is of
  `openkal.process` (clauses 7.6 and 7.18), which this package does not claim; nothing a
  program observes changes.
- `E2BIG` is `kal_err_no_space` in the shared translation (`src/common.h`), where it was
  `kal_err_io`: only `execve` answers it, which the sources built do not issue, and the
  source kept for `openkal.process` is thereby held to clause 7.6. Not run.

## 0.1.0

- openkal for UNIX System V Release 4 and its descendants on i386, on the kernel's
  system-call interface: seven interfaces of openkal 0.25 (`abort`, `stream`, `memory`,
  `env`, `time`, `fs`, `terminal`), standalone. The part of the interface the systems
  share is the one issued, so one executable runs on each; a call a system lacks is
  replaced by the older one or reported as not supported.
- Run under an emulation of the interface (ibcs-us 4.3 with corrections; UnixWare 7,
  Solaris and OpenServer 5), where the specification's suite holds in every observation
  made. Not run on a System V kernel.
