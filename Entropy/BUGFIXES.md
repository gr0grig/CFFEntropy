# Responsiveness audit — 2026-09-16

Reported trigger: moving the mouse over the chart when the red dot was present.
An indefinite host hang was not reproduced. The audit did reproduce unnecessary work in the mouse and section-selection paths and removed it.

## Changes

- Cache the static chart as a native bitmap. Mouse movement does not rebuild GDI+ geometry/fonts/gradient surfaces or calculate entropy.
- Invalidate only the old and new cursor strips. Vertical-only motion does not repaint; unchanged status text is not rewritten.
- Track mouse leave once per entry rather than on every movement. Clear the cursor on leaving the plot.
- Keep native bitmap ownership per dialog and release it on replacement/destruction.
- Coalesce ListView selection notifications: deselecting the old row no longer triggers a whole-file scan before selecting the new row. Full no longer scans twice.

## Evidence

Optimized MSVC x64 builds, same computer; timings are diagnostic samples, not universal guarantees.

| Operation | Before | After |
| --- | ---: | ---: |
| 100 chart repaints, 1100 × 500 | 296.94 ms | 16.22 ms |
| Switch between sections in a 256 MiB fixture | 81.61 ms | 25.99 ms |
| Full on the same fixture | 108.16 ms | 51.59 ms |

The final build passed parser/entropy tests, DLL export and dialog tests, scope/overlay/block-count/Full tests, Percent and absolute-offset checks, and mouse stress tests.

The mouse test executes 10,000 mouse moves with painting across 40 dialog lifecycle cycles. It checks cursor pixels, old-cursor removal, mouse leave, no entropy/chart rebuild during hover, no repaint on vertical-only motion, resizing, cache invalidation after block changes, pending selection messages at destruction, queue convergence and stable GDI object counts. Final run: 2.23 seconds. The rendered chart was also inspected.

Entropy calculation remains synchronous on open, Refresh and scope/block changes; very large files can still take time to scan. The original user-observed indefinite hang in CFF Explorer itself was not reproduced, so these results do not establish its unique root cause.
