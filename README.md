# QEMU EDU Linux PCI Driver

A Linux kernel driver for QEMU's EDU educational PCI device, written in C with a userspace ioctl test program.

The driver exposes `/dev/edu_lab0` for interrupt-driven factorial requests. It implements PCI binding, BAR0 MMIO, MSI interrupt handling, serialized hardware access, and bounded completion waits. DMA buffer management and request preparation are implemented; DMA transfers remain in progress.

This project targets an **emulated PCI endpoint** inside an x86-64 Linux guest. It does not access a physical GPU or demonstrate physical PCIe link behavior.

## Features and status

| Area | Current implementation |
| --- | --- |
| PCI lifecycle | EDU-only matching (`1234:11e8`), probe/remove, resource acquisition and failure unwind |
| MMIO | BAR0 mapping, identification read, verified liveness inversion |
| Userspace interface | Misc device, open/release callbacks, shared fixed-width ioctl structures |
| Factorial | Inputs 0–12, MSI notification, completion wait, result returned to userspace |
| Concurrency | One shared operation mutex serializes hardware requests |
| Failure handling | One-second factorial deadline; timeout sets FAILED and later valid hardware requests return `EIO` |
| DMA groundwork | 28-bit DMA mask, 4096-byte coherent allocation, DMA interrupt dispatch, payload staging and register setup |
| DMA round trip | Not implemented; valid requests currently return `EOPNOTSUPP` |

## Architecture

```text
Linux guest userspace
    test_ioctl.c
        |
        | open / ioctl / close
        v
Linux kernel
    /dev/edu_lab0 -> misc-device subsystem -> edu_fops
                                               |
                                               v
                                      per-device edu_device
                                      - BAR0 I/O mapping
                                      - operation mutex and state
                                      - factorial/DMA completions
                                      - coherent DMA buffer
                                               |
                                               | MMIO register access
                                               v
QEMU EDU virtual PCI device
    factorial computation / interrupt status / DMA engine
        |
        | MSI
        v
    Linux IRQ dispatch -> edu_irq_handler -> completion notification
```

The calling userspace thread executes the ioctl callback in kernel context. While it waits for completion, it can sleep so the CPU can run other work. The IRQ handler acknowledges EDU and signals the corresponding completion; the ioctl callback then verifies status and returns the result.

The normal factorial path transitions `READY -> BUSY -> READY` while holding the operation mutex. A timeout or inconsistent device status transitions to FAILED. There is no automatic FAILED-to-READY recovery.

## Repository layout

| File | Purpose |
| --- | --- |
| [edu_lab.c](edu_lab.c) | Kernel module, PCI lifecycle, MMIO, IRQ handling, ioctl implementation |
| [edu_ioctl.h](edu_ioctl.h) | Shared command definitions and request/response layouts |
| [test_ioctl.c](test_ioctl.c) | Userspace correctness and rejection tests |
| [Makefile](Makefile) | External-module Kbuild declaration |

## Build and run

### Requirements

- An x86-64 Linux guest with QEMU EDU attached using `-device edu`.
- Compiler/build tools and headers matching the running guest kernel.
- `pciutils` for device inspection and root privileges for module loading.
- A guest snapshot for experimental kernel testing.

For a Debian guest:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) pciutils git gdb
lspci -nn -d 1234:11e8
```

Run the following from the repository root **inside the Linux guest**.

### Build

```bash
make -C /lib/modules/$(uname -r)/build M="$PWD" modules
gcc -Wall -Wextra test_ioctl.c -o test_ioctl
```

### Load and test

Unload an older instance with `sudo rmmod edu_lab` first if it is already loaded. Rebuilding the file does not replace code in kernel memory.

```bash
sudo insmod ./edu_lab.ko
ls -l /dev/edu_lab0
sudo ./test_ioctl
echo $?
```

An exit status of **0** means all assertions passed. The test deliberately exercises rejected requests, so error descriptions such as “Bad address,” “Invalid argument,” and “Operation not supported” can appear during a passing run.

Inspect device configuration and kernel messages:

```bash
sudo lspci -vv -d 1234:11e8
grep edu_lab /proc/interrupts
sudo dmesg | tail -n 40
```

Expect `MSI: Enable+` and `BusMaster+` while loaded. IRQ numbers are allocated at runtime and must not be hardcoded.

### Unload

Close all device files and ensure the device is idle before unloading:

```bash
sudo rmmod edu_lab
sudo dmesg | tail -n 40
```

See the removal limitations below before attempting PCI unbinding or active-DMA teardown.

## Userspace API

Commands and layouts are defined in [edu_ioctl.h](edu_ioctl.h). The current supported pairing is x86-64 userspace with an x86-64 kernel; 32-bit compatibility is untested.

| Command | Request/response | Behavior |
| --- | --- | --- |
| `EDU_IOCTL_GET_INFO` | `struct edu_info` | ABI version 1, factorial limit 12, round-trip limit 0; reserved field zero |
| `EDU_IOCTL_FACTORIAL` | `struct edu_factorial` | Input 0–12, flags/reserved zero; returns result |
| `EDU_IOCTL_ROUNDTRIP` | `struct edu_roundtrip` | Length 1–4096 and flags zero; validation/setup only, then `EOPNOTSUPP` |

The ROUNDTRIP payload is a fixed 4096-byte array. Its kernel working copy is dynamically allocated rather than placed on the kernel stack.

Kernel callbacks return negative errors. Userspace sees `ioctl() == -1` with `errno` set:

| Error | Meaning in the current interface |
| --- | --- |
| `EFAULT` | A required userspace copy could not complete |
| `EINVAL` | Invalid input, length, flags, or reserved field |
| `ENOTTY` | Unknown ioctl command |
| `ENOMEM` | Request allocation failed |
| `ETIMEDOUT` | Started factorial request did not receive completion before its deadline |
| `EIO` | Device is not READY, or hardware status is inconsistent |
| `EOPNOTSUPP` | Valid ROUNDTRIP request; transfers not implemented |

GET_INFO describes implemented capabilities; it does not reset FAILED state. Closing and reopening the device does not recover it either.

## Validation

The following development runs passed in the Linux guest. Raw logs and complete environment records remain to be added to the repository.

| Check | Result |
| --- | --- |
| Identification and liveness | Identification `0x010000ed`; writing `0x55555555` returns `0xaaaaaaaa` |
| Repeated idle load/unload | Passed; exact historical cycle count not recorded |
| Factorial boundaries | `0! = 1`, `1! = 1`, `5! = 120`, `12! = 479001600` |
| Sequential reuse | 100 repetitions of four inputs: 400 loop requests on one descriptor |
| Two callers | Both processes exited 0; 800 loop requests plus individual cases |
| Eight callers | All processes exited 0; 3200 loop requests plus individual cases |
| Request rejection | Null pointers, unknown command, invalid factorial input/flags/reserved |
| ROUNDTRIP validation | Null pointer, lengths 0/4097, nonzero flags, and valid lengths 1/4096 with temporary unsupported result |
| Suppressed factorial notification | First valid request returned `ETIMEDOUT`; next returned `EIO` |
| Normal mode after suppression test | Normal suite passed after reload |

The multi-process runs validate results under those workloads; they do not measure overlap or prove that every request contended for the mutex. Missing-notification testing does not establish recovery from stalled hardware or validate late-interrupt timing.

### Completion-suppression test mode

The default-off, load-time parameter `test_suppress_completion` skips software completion signalling while still acknowledging interrupts:

```bash
sudo insmod ./edu_lab.ko test_suppress_completion=1
cat /sys/module/edu_lab/parameters/test_suppress_completion
```

The parameter should read `Y`. The first valid factorial request should return `ETIMEDOUT`; a subsequent valid request should return `EIO`.

The current userspace source contains a commented timeout-test block before the normal factorial cases. To run that check, enable that block and its early return, rebuild the test, and use a freshly loaded suppression-mode module. The ordinary success suite is not expected to pass with suppression enabled.

Restore the normal test source and reload without the parameter afterward. This controlled test allows EDU to finish computing; it suppresses notification rather than simulating an indefinitely active device.

## Milestone checklist

Checked items distinguish implemented behavior and completed development tests from the remaining work.

### 1. Bind, inspect, and detach

- [x] Match only EDU and implement module/PCI lifecycle callbacks.
- [x] Enable PCI memory access, claim/map BAR0, read identification, and verify liveness.
- [x] Track resources through initialization failures and idle teardown.
- [x] Exercise repeated idle module load/unload.
- [ ] Preserve logs, actual cycle count, BAR ranges, and interrupt capabilities.
- [ ] Publish a resource ownership table and debugging journal.
- [ ] Exercise initialization-failure paths through controlled fault injection.

### 2. Userspace contract

- [x] Publish `/dev/edu_lab0` with open/release/ioctl callbacks.
- [x] Define GET_INFO, FACTORIAL, and fixed-payload ROUNDTRIP structures.
- [x] Use checked user copies and validate supported inputs/flags.
- [x] Advertise factorial limit 12 and round-trip limit 0 through GET_INFO.
- [x] Dynamically allocate the large ROUNDTRIP working request.
- [x] Build a C userspace test program and verify success/rejection cases.
- [x] Document the current ABI, errors, and tested architecture scope.
- [ ] Extend or migrate the test program to C++ as planned.
- [ ] Return actual ROUNDTRIP data and advertise its supported limit when functional.

### 3. Interrupt-driven completion

- [x] Allocate one MSI vector, resolve its IRQ number, and register/free a handler.
- [x] Enable bus mastering for MSI delivery.
- [x] Verify artificial interrupt generation, delivery, acknowledgement, and completion during bring-up.
- [x] Replace the artificial trigger with actual factorial-completion interrupts.
- [x] Serialize complete hardware operations with a per-device mutex.
- [x] Prepare completion state and wait with a finite factorial deadline.
- [x] Dispatch factorial and DMA reason bits to separate completion objects.
- [x] Verify factorial boundaries, sequential reuse, and two/eight-process workloads.
- [x] Verify suppressed factorial completion produces timeout and later FAILED rejection.
- [ ] Exercise unrelated and deliberately late interrupt timing.

### 4. DMA round trip

- [x] Configure the default EDU 28-bit DMA mask.
- [x] Allocate/free a bounded coherent buffer with separate CPU and DMA addresses.
- [x] Validate ROUNDTRIP and stage payload bytes while holding the operation mutex.
- [x] Check DMA idle state, acknowledge old DMA status, synchronize the handler, and reset DMA completion.
- [x] Prepare source, destination, and count registers without starting a transfer.
- [ ] Establish safe buffer lifetime and teardown for uncertain DMA activity.
- [ ] Start host-to-EDU transfer and verify bounded completion.
- [ ] Clear/reuse the host buffer only after the first transfer is finished.
- [ ] Transfer EDU-to-host, return received bytes, and compare against the original payload.
- [ ] Verify coherency/ordering dependencies and DMA timeout handling.
- [ ] Test lengths 1, 3, 63, 64, 255, 4095, and 4096.
- [ ] Run seeded randomized transfers; suggested target 10,000, with actual settings recorded.
- [ ] Test concurrent and mixed factorial/ROUNDTRIP workloads.

### 5. Ownership, timeout, and removal

- [x] Implement mutex-protected READY, BUSY, and FAILED operation states.
- [x] Reject new hardware requests after factorial timeout or inconsistent status.
- [x] Confirm completion suppression and normal operation after test-mode reload.
- [ ] Add INITIALIZING/REMOVING lifecycle handling.
- [ ] Handle removal/open-file races with explicit object lifetimes and activity draining.
- [ ] Keep DMA memory alive until device access is known to have stopped.
- [ ] Document recovery/restart requirements for each uncertain state; no automatic recovery exists.
- [ ] Test teardown after failure and delayed completion without stale handler or memory access.

### Reproducibility and handoff

- [ ] Record QEMU and compiler/toolchain versions and the full guest launch configuration.
- [ ] Check in a state/data-flow/ownership design document and validation logs.
- [ ] Preserve at least one detailed debugging investigation.
- [ ] Document credits and licenses for reused material.
- [ ] Complete the core validation matrix and reproduce setup from a clean guest.

## Limitations

- This is an educational driver for a virtual PCI endpoint, not production hardware support.
- DMA registers are prepared, but the driver does not issue a DMA start command or return a round-trip payload.
- Removal currently assumes an idle device with all device files closed. Existing open files are not protected against PCI unbinding by a complete lifetime-management scheme.
- Safe active-DMA teardown and DMA timeout recovery are unfinished. Do not use the current unconditional buffer cleanup as evidence that uncertain device activity is safely quiesced.
- FAILED is persistent for the loaded device state. Reloading after the controlled suppression test is not a general recovery procedure for arbitrary hardware failure.
- Initialization fault injection, late-event tests, and 32-bit compatibility have not been validated.
- No physical PCIe bandwidth, link-training, or GPU-driver claims are made.

Scatter/gather DMA, arbitrary userspace-page pinning, userspace MMIO, INTx fallback, and nonblocking/poll interfaces are outside the initial core scope.

## Development environment

| Component | Recorded setting |
| --- | --- |
| Host | Windows |
| Guest | Debian x86-64 |
| Kernel | `6.12.107+deb13-amd64` |
| Header packages | Matching amd64/common headers, version `6.12.107-1` |
| QEMU | Version and full launch command to be recorded; EDU attached with `-device edu` |
| Toolchain | Version to be recorded |

Pin the tested versions when reproducing results. Physical GPU passthrough is not required.

## References

- [QEMU EDU specification](https://www.qemu.org/docs/master/specs/edu.html) — consult the version matching the installed QEMU.
- [Linux PCI driver guide](https://docs.kernel.org/PCI/pci.html).
- [External module build documentation](https://docs.kernel.org/kbuild/modules.html).
- [Device I/O and MMIO ordering](https://docs.kernel.org/driver-api/device-io.html).
- [MSI driver guide](https://docs.kernel.org/PCI/msi-howto.html).
- [Completions](https://docs.kernel.org/scheduler/completion.html).
- [DMA API guide](https://docs.kernel.org/core-api/dma-api-howto.html).

Review API details against the headers and documentation for the tested guest kernel.
