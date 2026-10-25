Regression and Fuzzing Infrastructure

This directory contains the automated testing infrastructure for `wterm`.
It emphasizes memory safety verification, edge-case fault tolerance, 
and logical correctness via fuzzing (AFL) and isolated unit tests.

ARCHITECTURE OVERVIEW:
	The regression suite uses a mocking strategy to isolate the C modules 
	from the underlying operating system.

	create_chaos.c / .h
	    Custom fault-injection wrappers. Simulates system stress and edge 
	    cases by randomly forcing standard C library functions (malloc, 
	    strdup, getcwd) to fail, testing out-of-memory (OOM) handling and 
	    null-pointer dereferences. 
	    It also forces extreme values into environmental variables (e.g., PATH 
	    and PS1) to test parser resilience against structural noise.

	redef.c
	    A macro-based redirection layer. Overrides OS-level calls like 
	    poll(2), ioctl(2) (specifically TIOCGWINSZ for terminal resizing), 
	    and sigaction. Allows fuzzing the asynchronous logic synchronously 
	    without relying on actual terminal I/O.

UNIT TESTS:
	The `unit_tests/` directory isolates core components for targeted fuzzing.

	* check_exec_safety/
	    Verifies the boundary checks of command buffers before they are 
	    dispatched to execve(2).
	    - Mocks execve to validate dynamic OS limits (ARG_MAX) and IPC string 
	      construction.
	    - Uses pre-generated corpuses (e.g., oversized commands, missing 
	      paths) to ensure size_t limits are never exceeded.
	    - Validates that the internal string concatenation logic safely handles 
	      path variables alongside the IPC command injection (`;set >&3`).

	* parse_env/
	    Fuzzes the custom IPC environment variable parser (file descriptor 3).
	    - Runs in a persistent AFL loop (`__AFL_LOOP`) for maximum execution 
	      speed.
	    - Validates the internal hash table against memory leaks and buffer 
	      overflows when parsing chunks.
	    - Enforces strict shell-variable formatting rules (alnum variable names, 
	      single-quote enclosures) while ignoring malicious or corrupted chunks 
	      injected into the IPC pipeline.

SYSTEM-LEVEL VALIDATION:
        Prior to the implementation of the LLVM-based fuzzing infrastructure,
        the core of `wterm` underwent cumulative, multi-day stress testing on 
        OpenBSD. It was executed under the strict `MALLOC_OPTIONS=S` environment 
        to leverage OpenBSD's hardened memory allocator, rigorously verifying the 
        absence of use-after-free vulnerabilities, double-frees, and uninitialized 
        memory bugs.

EXECUTION INSTRUCTIONS:
	The infrastructure relies heavily on LLVM instrumentation.
	1. Navigate to the desired test directory (e.g., `parse_env/`).
	2. Run the provided `afl_test` script.
	3. The script will automatically compile the targets using `afl-clang-fast`
	   with AddressSanitizer (ASAN) enabled, and launch a parallel AFL cluster 
	   (master/slave nodes) to begin the fuzzing campaign.
