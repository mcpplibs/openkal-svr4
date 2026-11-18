# Changelog

## 0.1.0

- openkal for UNIX System V Release 4 and its descendants on i386, on the kernel's
  system-call interface: seven interfaces of openkal 0.25 (`abort`, `stream`, `memory`,
  `env`, `time`, `fs`, `terminal`), standalone. The part of the interface the systems
  share is the one issued, so one executable runs on each; a call a system lacks is
  replaced by the older one or reported as not supported.
- Run under an emulation of the interface (ibcs-us 4.3 with corrections; UnixWare 7,
  Solaris and OpenServer 5), where the specification's suite holds in every observation
  made. Not run on a System V kernel.
