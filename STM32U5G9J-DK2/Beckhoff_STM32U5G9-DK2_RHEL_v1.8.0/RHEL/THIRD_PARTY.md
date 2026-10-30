# Dependency and provenance statement

The project does not fetch or link the C++ EIPScanner library.

The required EtherNet/IP Scanner/Originator behaviour is implemented by the native-C modules under:

```text
include/eip/
src/eip/
```

The retained scanner modules originate from the user-supplied EK9500 Terminal Suite C project. Removed subsystems include discovery, IPC, monitor applications and persistence.

Runtime dependencies are limited to the ISO C library, POSIX sockets, POSIX threads and the standard math library.
