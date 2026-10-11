# QEMU EDU Linux PCI Driver

## Overview

This educational project builds a Linux kernel PCI driver for QEMU's EDU virtual device. It explores PCI device discovery, memory-mapped I/O (MMIO), interrupts, DMA, synchronization, resource ownership, and failure handling.

The planned finished system consists of a C kernel module and a C++ userspace test tool. The tool will request factorial computations and byte-buffer round trips through the driver. The userspace interface, interrupt handling, and DMA are not implemented yet.

EDU is an emulated educational PCI endpoint, not a physical GPU or PCIe card. The Linux guest runs in QEMU on a Windows host with EDU attached using `-device edu`.

## Current status

**Milestone 1's functional work is complete:** the module binds to EDU, reads its identification register, verifies its liveness register, and detaches while idle. Repeated load/unload testing has been reported successful. The exact cycle count and saved logs still need to be recorded in the repository.

The status below reflects development and testing in the Linux guest. Successful normal-path tests do not establish that every initialization-failure branch has been exercised.

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

### 2. Userspace contract — not started

- [ ] Expose a misc/character device such as `/dev/edu_lab0`.
- [ ] Define a versioned ioctl ABI using fixed-width fields and initialized padding.
- [ ] Define GET_INFO, FACTORIAL, and ROUNDTRIP requests.
- [ ] Validate commands, flags, lengths, factorial inputs, and userspace copies.
- [ ] Define consistent error results and build a C++ test tool.

### 3. Interrupt-driven completion — not started

- [ ] Request an interrupt vector and register a handler; target MSI first.
- [ ] Verify interrupt generation and acknowledgement.
- [ ] Serialize operations with a process-context mutex.
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
- [ ] Test invalid user addresses and unknown ioctls.
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

Build the module:

```bash
make -C /lib/modules/$(uname -r)/build M="$PWD" modules
```

Load, inspect, and unload:

```bash
sudo insmod ./edu_lab.ko
sudo dmesg | tail -n 30
sudo rmmod edu_lab
sudo dmesg | tail -n 30
```

Unload an older loaded version before testing a rebuilt module. Rebuilding changes the disk file, not the module already loaded in memory. Take a guest snapshot before experimental kernel testing.

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

Current behavior is limited to probe-time register checks and idle removal. There is no userspace device interface, factorial request path, interrupt handler, DMA transfer path, or concurrent-operation support yet.

This project demonstrates Linux driver mechanisms against emulated hardware. It does not validate physical PCIe signaling, link training, production GPU behavior, or physical-device performance.

## References

- [QEMU EDU device specification](https://www.qemu.org/docs/master/specs/edu.html) — consult the specification matching the tested QEMU version.
- [Linux PCI driver guide](https://docs.kernel.org/PCI/pci.html).
- [Building external kernel modules](https://docs.kernel.org/kbuild/modules.html).
- [Linux device I/O accessors](https://docs.kernel.org/driver-api/device-io.html).

Review APIs against the installed headers for the pinned guest kernel.
