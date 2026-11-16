# QEMU EDU Linux PCI Driver

## Overview

This educational project builds a Linux kernel PCI driver for QEMU's EDU virtual device. It explores PCI device discovery, memory-mapped I/O (MMIO), interrupts, DMA, synchronization, resource ownership, and failure handling.

The planned finished system consists of a C kernel module and a userspace test tool for factorial computations and byte-buffer round trips. A C test program currently exercises the ioctl interface. Factorial execution, interrupt handling, and DMA transfers are not implemented yet; a C++ test tool remains a roadmap target.

EDU is an emulated educational PCI endpoint, not a physical GPU or PCIe card. The Linux guest runs in QEMU on a Windows host with EDU attached using `-device edu`.

## Current status

**Milestone 1's functional work is complete:** the module binds to EDU, reads its identification register, verifies its liveness register, and detaches while idle. Repeated load/unload testing has been reported successful. The exact cycle count and saved logs still need to be recorded in the repository.

**Milestone 2 is in progress:** `/dev/edu_lab0` exposes open/release and ioctl callbacks. GET_INFO returns ABI version 1. FACTORIAL requests are copied and validated but return `EOPNOTSUPP` instead of computing a result. The userspace validation suite has returned exit status 0.

**Milestone 3 has started:** the guest driver allocates one MSI vector and resolves its Linux IRQ number. `lspci` has shown `MSI: Enable+`; the observed Linux IRQ number was 31. This number is assigned at runtime and must not be hardcoded. No interrupt handler is registered yet, and interrupt delivery has not been tested.

The status below includes development and test results reported from the Linux guest. The inspected Windows checkout does not yet contain the latest guest-side vector-allocation changes or the module-registration return-value fix; synchronize those source changes before reproducing the MSI result from this checkout. Successful normal-path tests do not establish that every initialization-failure branch has been exercised.

## Scope

The core target is one device, one outstanding hardware operation, synchronous userspace requests, and coherent DMA buffers. Scatter/gather DMA, arbitrary user-page pinning, userspace MMIO, and physical GPU access are outside the initial scope.

## Milestones

### 1. Bind, inspect, and detach — functional work complete

- [x] Set up an x86-64 Linux QEMU guest and confirm EDU through `lspci`.
- [x] Build a loadable kernel module with Kbuild.
- [x] Register a PCI driver matching vendor/device IDs `1234:11e8`.
- [x] Implement probe and remove callbacks.
- [x] Allocate per-device state and retain it through PCI driver data.
- [x] Enable the device, claim BAR0, and map its MMIO region.
- [x] Read EDU identification at offset `0x00`; observed value: `0x010000ed`.
- [x] Write `0x55555555` to the liveness register at offset `0x04`, read `0xaaaaaaaa`, and compare against the expected inverse.
- [x] Implement partial-initialization cleanup and reverse-order teardown.
- [x] Exercise repeated idle module load/unload.
- [ ] Save test logs, the actual cycle count, and complete environment details.
- [ ] Record the resource ownership table and a debugging journal entry.
- [ ] Exercise initialization-failure paths; current cleanup behavior has been reviewed but not fault-injection tested.

### 2. Userspace contract — in progress

- [x] Expose `/dev/edu_lab0` through the misc-device subsystem.
- [x] Implement open, release, and ioctl callbacks.
- [x] Recover per-device state with `container_of` in open and retain it through `file->private_data`.
- [x] Define shared GET_INFO and FACTORIAL commands with fixed-width request/response fields.
- [x] Implement GET_INFO with ABI version 1, zero unsupported-operation limits, and zero reserved fields.
- [x] Use checked `copy_to_user` and `copy_from_user` operations.
- [x] Reject unknown commands, invalid factorial inputs, and nonzero flags/reserved fields.
- [x] Build a C userspace test tool and verify the current success/rejection cases.
- [ ] Define the ROUNDTRIP request and validate its length and flags.
- [ ] Document the complete ABI and error contract.
- [ ] Extend or migrate the userspace tool to C++ as planned.

### 3. Interrupt-driven completion — in progress

- [x] Add and initialize a per-device operation mutex; protect the temporary ioctl register read.
- [x] Allocate one MSI vector, resolve its Linux IRQ number, and add vector cleanup (guest-side changes; source synchronization pending).
- [x] Observe `MSI: Enable+` while the guest module is loaded.
- [ ] Register an interrupt handler and pair it with handler cleanup.
- [ ] Verify interrupt generation and acknowledgement.
- [ ] Extend mutex protection over the complete hardware operation and verify concurrent callers.
- [ ] Establish completion state before starting work and wait with a finite timeout.
- [ ] Complete factorial requests through interrupts.
- [ ] Prevent unrelated or late interrupts from completing the wrong request.

### 4. DMA round trip — not started

- [ ] Negotiate EDU's DMA mask and enable bus mastering.
- [ ] Allocate a bounded coherent DMA buffer using the DMA API.
- [ ] Transfer data to EDU's buffer and back, waiting for each transfer to finish.
- [ ] Use device-visible DMA addresses and appropriate ordering.
- [ ] Verify byte-for-byte results for patterned, randomized, and boundary-sized payloads.

### 5. Ownership, timeout, and removal — not started

- [ ] Define INITIALIZING, READY, BUSY, FAILED, and REMOVING states.
- [ ] Block new operations when a timeout leaves device activity uncertain.
- [ ] Keep DMA memory alive until activity is finished or safely quiesced.
- [ ] Handle removal/open-file races and object lifetimes.
- [ ] Document EDU's recovery limitations and accurately label test-only fault injection.

### Final validation and handoff — pending

- [ ] Test factorial inputs 0, 1, 5, and 12, plus invalid inputs.
- [ ] Test round-trip lengths 1, 3, 63, 64, 255, 4095, and 4096, plus invalid lengths.
- [x] Test null pointers for GET_INFO/FACTORIAL and an unknown ioctl command.
- [ ] Extend invalid-address testing beyond null pointers and cover future commands.
- [ ] Run seeded randomized round trips and record the actual count.
- [ ] Verify serialization with concurrent caller processes.
- [ ] Exercise suppressed-completion timeout behavior and repeated idle detach.
- [ ] Publish reproducible setup instructions, logs, design notes, and limitations.

## Current build and run

Run these commands inside the Linux guest, from the directory containing `edu_lab.c` and `Makefile`. Install compiler/build tools and headers matching the running kernel first.

The current Makefile specifies:

```makefile
obj-m := edu_lab.o
```

Build the module and current C userspace test:

```bash
make -C /lib/modules/$(uname -r)/build M="$PWD" modules
gcc -Wall -Wextra test_ioctl.c -o test_ioctl
```

Load, inspect, and unload:

```bash
sudo insmod ./edu_lab.ko
sudo dmesg | tail -n 30
ls -l /dev/edu_lab0
sudo lspci -vv -d 1234:11e8
sudo ./test_ioctl
echo $?
sudo rmmod edu_lab
sudo dmesg | tail -n 30
```

Unload an older loaded version before testing a rebuilt module. Rebuilding changes the disk file, not the module already loaded in memory. Take a guest snapshot before experimental kernel testing.

Inspect MSI status before unloading. After synchronizing and building the guest-side vector changes, expect `MSI: Enable+`. This confirms configuration, not interrupt delivery.

The test program should return exit status `0`. It intentionally prints errors for rejected requests; those messages are expected when their error codes match the tests.

## Current validation results

The following cases have been reported passing in the Linux guest:

| Request | Expected result |
| --- | --- |
| Valid GET_INFO | ABI version 1; factorial limit, round-trip limit, and reserved field all zero |
| GET_INFO with null destination | `EFAULT` |
| Unknown command | `ENOTTY` |
| Valid GET_INFO after rejected requests | Correct response, overwriting sentinel field values |
| FACTORIAL with null request pointer | `EFAULT` |
| FACTORIAL input 13 | `EINVAL` |
| FACTORIAL with nonzero flags | `EINVAL` |
| FACTORIAL with nonzero reserved field | `EINVAL` |
| Valid FACTORIAL request | `EOPNOTSUPP` until execution is implemented |

Kernel handlers return negative error codes. In userspace, ioctl returns `-1` and sets `errno` to the corresponding positive error constant. GET_INFO must continue advertising zero factorial support until computation is functional.

## Next step

Define and register an interrupt handler using the resolved Linux IRQ number. First verify EDU's interrupt status/raise/acknowledge flow independently of factorial. Then introduce bounded completion waiting and connect interrupts to factorial execution. No test interrupt should be generated before handler registration succeeds.

## Environment

| Item | Tested setting |
| --- | --- |
| Host | Windows |
| Guest | Debian, x86-64 |
| Guest kernel | `6.12.107+deb13-amd64` |
| Kernel header packages | Matching amd64 and common headers, version `6.12.107-1` |
| QEMU version | To be recorded |
| QEMU launch configuration | Includes `-device edu`; full command to be recorded |

## Limitations

Current behavior includes probe-time register checks, a userspace ioctl interface, and a mutex-protected diagnostic register read for validated FACTORIAL requests. These requests do not start a computation or return a factorial result. There is no interrupt handler, completion-wait path, or DMA transfer path yet. Concurrent hardware-operation behavior has not been validated.

Removal is currently limited to idle use with all device files closed. Misc deregistration prevents new opens but does not invalidate existing file references. Safe PCI unbinding with open files requires additional device-state lifetime management; the file-operations module owner alone does not solve it.

This project demonstrates Linux driver mechanisms against emulated hardware. It does not validate physical PCIe signaling, link training, production GPU behavior, or physical-device performance.

## References

- [QEMU EDU device specification](https://www.qemu.org/docs/master/specs/edu.html) — consult the specification matching the tested QEMU version.
- [Linux PCI driver guide](https://docs.kernel.org/PCI/pci.html).
- [Building external kernel modules](https://docs.kernel.org/kbuild/modules.html).
- [Linux device I/O accessors](https://docs.kernel.org/driver-api/device-io.html).

Review APIs against the installed headers for the pinned guest kernel.
