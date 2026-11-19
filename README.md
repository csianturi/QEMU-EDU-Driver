# QEMU EDU Linux PCI Driver

## Overview

This educational project builds a Linux kernel PCI driver for QEMU's EDU virtual device. It explores PCI device discovery, memory-mapped I/O (MMIO), interrupts, DMA, synchronization, resource ownership, and failure handling.

The planned finished system consists of a C kernel module and a userspace test tool for factorial computations and byte-buffer round trips. A C test program currently exercises GET_INFO, request validation, and interrupt-driven factorial execution. Input 5 has returned the expected result 120 in the Linux guest. DMA transfers are not implemented yet; a C++ test tool remains a roadmap target.

EDU is an emulated educational PCI endpoint, not a physical GPU or PCIe card. The Linux guest runs in QEMU on a Windows host with EDU attached using `-device edu`.

## Current status

**Milestone 1's functional work is complete:** the module binds to EDU, reads its identification register, verifies its liveness register, and detaches while idle. Repeated load/unload testing has been reported successful. The exact cycle count and saved logs still need to be recorded in the repository.

**Milestone 2 is in progress:** `/dev/edu_lab0` exposes open/release and ioctl callbacks. GET_INFO returns ABI version 1. FACTORIAL requests validate input 0-12 and return a hardware-computed result through checked user copies. ROUNDTRIP is not defined yet. GET_INFO still advertises a zero factorial limit in the current source and must be updated alongside its tests after factorial validation.

**Milestone 3 is in progress:** the driver allocates one MSI vector, resolves its Linux IRQ number, registers a handler, and enables bus mastering. The earlier one-shot probe test verified delivery, acknowledgement, and completion signalling. That artificial trigger/wait is now inactive; actual factorial requests use the completion with a one-second deadline under the operation mutex. The handler signals only for the factorial interrupt bit. `lspci` has shown `MSI: Enable+` and `BusMaster+`; the observed Linux IRQ number was 31 and must not be hardcoded. Boundary, repeated-operation, concurrent-caller, and timeout validation remain pending.

The status below includes source inspection and test results reported from the Linux guest. This checkout includes the factorial execution path and a userspace check for `5! == 120`. The earlier rejection suite was reported passing before computation was added; rerun the complete updated suite to establish a new baseline. Successful normal-path tests do not establish that every initialization-failure or timeout branch has been exercised.

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
- [x] Return the computed factorial result to userspace; input 5 verified as 120.
- [ ] Advertise factorial limit 12 in GET_INFO and update corresponding userspace expectations after validation.
- [ ] Define ROUNDTRIP with a fixed payload array, lengths 1-4096, and flag validation; allocate large request/bounce objects outside the kernel stack.
- [ ] Document the complete ABI and error contract.
- [ ] Explicitly document the tested x86-64 userspace/kernel pairing; do not claim untested 32-bit compatibility.
- [ ] Extend or migrate the userspace tool to C++ as planned.

### 3. Interrupt-driven completion — in progress

- [x] Add and initialize a per-device operation mutex covering request preparation, hardware execution, waiting, and result collection.
- [x] Allocate one MSI vector, resolve its Linux IRQ number, and add vector cleanup.
- [x] Observe `MSI: Enable+` while the guest module is loaded.
- [x] Enable bus mastering for device-initiated MSI writes and pair it with teardown.
- [x] Register an interrupt handler and pair it with handler cleanup.
- [x] Verify one-shot interrupt generation, delivery, and acknowledgement during probe.
- [x] Initialize a per-device completion before handler registration; signal it after interrupt acknowledgement.
- [x] Verify the earlier one-shot probe-time completion wait with a one-second deadline; artificial trigger/wait now inactive.
- [x] Prepare each request by checking hardware idle, clearing pending status, synchronizing handler execution, and reinitializing completion.
- [x] Establish BUSY state before starting computation and wait with a finite timeout.
- [x] Complete a factorial request through interrupts and return the verified result for input 5.
- [x] Filter factorial interrupt reasons and reject new hardware requests after entering FAILED.
- [ ] Verify repeated operations, boundary inputs, and concurrent callers.
- [ ] Exercise missing completion, unrelated events, and late completion; verify they cannot falsely complete a later request.

### 4. DMA round trip — not started

- [x] Enable bus mastering for MSI delivery.
- [ ] Negotiate EDU's DMA mask and audit bus-master lifetime for DMA operations.
- [ ] Allocate a bounded coherent DMA buffer using the DMA API.
- [ ] Transfer data to EDU's buffer and back, waiting for each transfer to finish.
- [ ] Do not clear/reuse the host buffer until host-to-device transfer completion; return actual received bytes and compare them in userspace.
- [ ] Use device-visible DMA addresses and appropriate ordering.
- [ ] Verify byte-for-byte results for patterned, randomized, and boundary-sized payloads.

### 5. Ownership, timeout, and removal — partial state handling implemented

- [x] Add READY, BUSY, and FAILED operation states, accessed in process context under the operation mutex after publication.
- [x] Mark FAILED on timeout or inconsistent hardware status; reject subsequent valid hardware requests with `EIO`.
- [ ] Add INITIALIZING/REMOVING lifecycle handling and document process/interrupt-context ownership.
- [ ] Test timeout-to-FAILED behavior and later-request rejection; no FAILED-to-READY recovery is implemented.
- [ ] Keep DMA memory alive until activity is finished or safely quiesced.
- [ ] Handle removal/open-file races and object lifetimes.
- [ ] Document EDU's recovery limitations and accurately label test-only fault injection.

### Final validation and handoff — pending

- [ ] Test factorial inputs 0, 1, 5, and 12, plus invalid inputs.
- [x] Observe the correct result for input 5: 120.
- [ ] Test round-trip lengths 1, 3, 63, 64, 255, 4095, and 4096, plus invalid lengths.
- [x] Test null pointers for GET_INFO/FACTORIAL and an unknown ioctl command.
- [ ] Extend invalid-address testing beyond null pointers and cover future commands.
- [ ] Test round-trip lengths 0 and 4097 for defined rejection.
- [ ] Run seeded randomized round trips (suggested target 10,000; adapt and record the actual count).
- [ ] Verify serialization with concurrent caller processes (suggested 2-8; record the actual workload).
- [ ] Exercise suppressed-completion timeout behavior and repeated idle detach.
- [ ] Publish reproducible setup instructions, logs, design notes, and limitations.
- [ ] Record compiler/toolchain and QEMU versions, BAR ranges, interrupt capabilities, and a detailed debugging investigation.
- [ ] Include a state/data-flow/ownership design document and credits/licenses for reused material.

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
grep edu_lab /proc/interrupts
sudo ./test_ioctl
echo $?
sudo rmmod edu_lab
sudo dmesg | tail -n 30
```

Unload an older loaded version before testing a rebuilt module. Rebuilding changes the disk file, not the module already loaded in memory. Take a guest snapshot before experimental kernel testing.

Inspect PCI status before unloading: expect `MSI: Enable+` and `BusMaster+`. Configuration alone does not prove delivery; interrupt logs, counter changes during factorial requests, and returned results provide additional evidence.

The artificial probe-time interrupt trigger is no longer active. A valid FACTORIAL ioctl writes its input to offset `0x08` after preparing completion state and enabling factorial notifications via device-status bit `0x80`. EDU computes, stores the result, clears computing bit `0x01` at offset `0x20`, and raises an interrupt. The handler reads interrupt status (`0x24`), acknowledges through `0x64`, and signals `op_done` when the factorial reason bit is present. The ioctl waits using `wait_for_completion_timeout` with `msecs_to_jiffies(1000)`, verifies hardware is no longer computing, and returns the result.

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
| Valid FACTORIAL request, input 5 | Success; result 120 (new guest-reported computation test) |

Kernel handlers return negative error codes. In userspace, ioctl returns `-1` and sets `errno` to the corresponding positive error constant. Factorial now executes, but GET_INFO and its tests still use a zero factorial limit; update both to 12 after the boundary/repeated-request validation. DMA support remains zero.

### Interrupt/completion bring-up result

The reported successful guest run included:

```text
Linux IRQ vector: 31
EDU interrupt status register: 0x00000001
IRQ completion received; 250 jiffies remaining
```

The final line summarizes the observed positive wait result. It is **remaining timeout time**, not elapsed execution time or a latency measurement. A successful wait does not establish whether the calling thread actually slept: an early `complete()` is remembered and lets the wait return immediately.

This historical result is from the one-shot probe-time test, not the current userspace computation path. Deliberately suppressed or delayed completion has not been reported tested. A timeout stops waiting; it does not cancel an interrupt or terminate a handler. In the current ioctl path, timeout sets FAILED and returns `ETIMEDOUT` without freeing device resources. Later hardware requests return `EIO`, even if the old interrupt eventually arrives. Teardown frees the handler before the mapping and state so an executing handler can finish safely.

## Next step

Extend userspace factorial correctness tests to inputs 0, 1, 5, and 12, expecting 1, 1, 120, and 479001600. Repeat the sequence on the same loaded driver to exercise completion reuse and READY/BUSY transitions. Rerun all rejection tests and save the new baseline. Then update GET_INFO's factorial limit and its tests to 12. Follow with concurrent-caller and controlled timeout/late-event validation before starting DMA work.

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

Current behavior includes probe-time register checks, a userspace ioctl interface, and serialized interrupt-driven factorial execution. Input 5 has been verified; boundary inputs, repeated requests, concurrent callers, and timeout/late-event behavior remain unvalidated. The handler filters factorial reasons but has no hardware request identifier; late-event safety relies on serialization, idle preparation, and refusing further hardware operations after FAILED. No automatic recovery or DMA transfer path exists yet.

Removal is currently limited to idle use with all device files closed. Misc deregistration prevents new opens but does not invalidate existing file references. Safe PCI unbinding with open files requires additional device-state lifetime management; the file-operations module owner alone does not solve it.

This project demonstrates Linux driver mechanisms against emulated hardware. It does not validate physical PCIe signaling, link training, production GPU behavior, or physical-device performance.

## References

- [QEMU EDU device specification](https://www.qemu.org/docs/master/specs/edu.html) — consult the specification matching the tested QEMU version.
- [Linux PCI driver guide](https://docs.kernel.org/PCI/pci.html).
- [Building external kernel modules](https://docs.kernel.org/kbuild/modules.html).
- [Linux device I/O accessors](https://docs.kernel.org/driver-api/device-io.html).
- [Linux MSI driver guide](https://docs.kernel.org/PCI/msi-howto.html).
- [Linux completion documentation](https://docs.kernel.org/scheduler/completion.html).

Review APIs against the installed headers for the pinned guest kernel.
