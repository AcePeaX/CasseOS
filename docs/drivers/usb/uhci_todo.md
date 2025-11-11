# UHCI Driver TODOs

## Core Controller/Transfer Handling
- [ ] Implement bulk and isochronous transfers; currently only control and a fixed interrupt endpoint are used.
- [ ] Add support for multiple interrupt devices (shared tree of QHs and periodic schedule balancing).
- [ ] Reuse TD/QH allocations instead of re-allocating for every control transfer; add a pool and free list.
- [ ] Handle error recovery: halt queues, clear status, and reset endpoints/ports when TDs report STALL/CRC errors.
- [ ] Implement a software frame list builder for periodic/non-periodic schedules instead of directly overwriting `frame_list` entries.
- [ ] Support low-speed devices behind UHCI (currently assumes full-speed keyboard only).

## Enumeration / Hub Awareness
- [ ] Enumerate devices behind external USB hubs (handle hub class requests, port power, status changes).
- [ ] Track device addresses dynamically; support disconnect/reconnect instead of assuming `port+1`.
- [ ] React to port change interrupts (connect/disconnect/resume) instead of polling once at boot.

## Power Management & Reset
- [ ] Implement proper controller reset sequence when errors occur or when shutting down.
- [ ] Support suspend/resume of ports and global/port power control per the UHCI spec.

## Interrupt / ISR
- [ ] Move debug prints out of ISR path and add proper bottom-half/tasklet context.
- [ ] Implement masking/acknowledging of specific UHCI interrupts (USBINT, USBERRINT, etc.) and escalate severe faults.

## Descriptor/Transfer Layer
- [ ] Generalize HID support beyond boot keyboards (report descriptors, parsing, etc.).
- [ ] Add support for OUT interrupt endpoints and feature reports for LEDs.
- [ ] Provide a higher-level API to submit arbitrary control/bulk requests to class drivers.

## Testing & Tooling
- [ ] Create unit tests or QEMU scripts to validate TD rearming, toggle handling, and error paths.
- [ ] Add tracing hooks or buffers for debugging without spamming the serial console.
