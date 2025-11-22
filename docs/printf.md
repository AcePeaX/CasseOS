## Kernel `printf` Reference

The screen driver implements a self-contained `printf` that mirrors the hosted C
library version closely so firmware differences (BIOS vs UEFI) do not affect
formatting. The formatter streams into the console through 256-byte chunks, so
callers can print arbitrarily long strings without stack-heavy buffers.

### Supported syntax

```
%[flags][width][.precision][length]specifier
```

- **Flags:** `-` (left align), `+` (always show sign), space (leading space for
  positive numbers), `#` (alternate form: `0`, `0x`, `0b`, etc.), `0`
  (zero-padding when width specified).
- **Width:** decimal literal or `*` (next `int` argument). Negative widths imply
  `-` flag.
- **Precision:** `.` followed by literal or `*`. Applies to integers, strings,
  and floats. Negative precision disables the precision override.
- **Length modifiers:** `hh`, `h`, `l`, `ll`, `z`, `t`, `j`, `L`.
- **Specifiers:** `d`/`i`, `u`, `x`/`X`, `o`, `b`/`B` (binary), `p`, `c`, `s`,
  `f`/`F`, and `n`. `%` emits a literal percent.

Special notes:

- `%p` always prints `0x`-prefixed lowercase hex regardless of other flags.
- `%b`/`%B` are non-standard binary helpers and respect `#` for `0b`/`0B`.
- `%n` stores the number of characters emitted so far into the provided pointer,
  honoring length modifiers (e.g., `%hn`, `%lln`).
- `%f`/`%F` supports precision up to 32 digits after the decimal point. NaN and
  ±Inf render as `nan`/`inf` (`NAN`/`INF` with `%F`) with optional sign/space.
- Strings accept `NULL`, printing `(null)` to aid debugging.

Because we always work with identity-mapped memory under both boot paths, all
formatting code is safe to use once interrupts are enabled; no heap allocation
is needed.*** End Patch
