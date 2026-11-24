# Reflex Adapt branch and release policy

This fork retains **upstream-first main** and two standalone feature branches.
Do not use the published firmware tag as the main branch tip or merge local
features into main.

## Long-lived branches

| Branch | Purpose |
| --- | --- |
| `main` | Fast-forward-only mirror of `misteraddons/reflex-io/main` |
| `feature/hkt-7500-twin-stick` | Dreamcast HKT-7500 controller, routing, and Pause remapping; upstream PR #2 |
| `feature/guide-hold-duration` | Global Start-to-Guide hold duration, Adapt Web selector, hardware OLED System Settings selector |
| `develop` | Single combined candidate integrating both features and versioning for Classic2USB 1.0.3-RC1 |

No other feature or integration branches are part of the active workflow.
The separate `feature/classic2usb-v1.0.3-guide-hold` branch is superseded by
`develop` and should be deleted once the default branch is corrected.

## Important behavior distinctions

- Upstream main uses a fixed 1,000 ms Hold Start shortcut.
- The Twin Stick feature has **no special override** of that upstream
  behavior. Its own 1,000 ms override was removed.
- The Guide Hold feature replaces the Classic2USB fixed duration with one
  global 500-3,000 ms option (default 2,000 ms); no Twin Stick-only exception.
- `develop` combines both features; the standard Classic2USB OLED Home screen
  is retained. No custom Twin Stick Home artwork is included.
- The global Hotkeys On/Off remains authoritative; Menu Hold is separate
  and retains its 500 ms default.

## Update and release discipline

1. Check upstream changes, then fast-forward `main` from `upstream/main`
   only; never squash, rebase, or merge downstream features into main.
2. Advance each feature only with commits belonging to that feature.
   Preserve upstream PR #2 on the Twin Stick branch.
3. Update the single `develop` integration branch to include both feature
   sets, resolve incompatibilities there, and run regression tests.
4. Build Classic2USB 1.0.3-RC1 **only from develop**. Record exact source
   commit, embedded version, build ID, UF2 SHA-256, memory use, and tests.
5. Validate XInput and DInput Twin Stick behavior, hardware MODE > System
   Settings > Guide Hold, Adapt Web settings read/write, persistence, global
   Hotkeys On/Off, and all six hold durations.
6. Do not publish a final v1.0.3 release until the hardware UAT passes and
   Adapt Manager's RC-versus-final version ordering is verified.

The existing published `v1.0.2` tag and release remain immutable and distinct
from the upstream-synchronized `main`. The earlier custom OLED RC1 firmware
is superseded and must not be mistaken for the next combined candidate.

## GitHub housekeeping

- Set the repository default branch to `main` in GitHub Settings.
- Keep upstream PR #2 open. Fork PR #1 was redundant and has been closed.
- Delete only the superseded `feature/classic2usb-v1.0.3-guide-hold` ref
  once `develop` is verified. Never delete the published `v1.0.2` tag.
