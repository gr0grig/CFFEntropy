# Entropy for CFF Explorer

An x64 native CFF Explorer extension showing Shannon entropy for every PE section and any overlay. Uses the installed Explorer Suite SDK headers under `../sdk/UPX_Utility`.

Table columns: section number, section/region name, file offset, exact declared raw size in bytes, readable size, Percent, and entropy in bits per byte. Percent is raw size divided by the entire current file size times 100, with two decimal places; it also applies to the overlay. Readable sizes use 1024-based B/KB/MB/GB/TB units, rounded to two decimals above bytes. Overlay has no section number. Empty raw sections display N/A entropy. Truncated data is reported below the table and entropy is calculated from available bytes only.

Entropy is `-sum(p * log2(p))`, from 0 to 8. It is calculated from raw file bytes, including raw section padding, without virtual zero filling. Overlay starts after the maximum of the header extent and all nonempty section raw ends. Trailing certificates are included. Inter-section gaps are not separate rows. No overlay row is shown when no trailing bytes exist.

The extension reads the current document buffer and never modifies it. Use Refresh after editing. Calculation is synchronous and may take time on large files. Mouse movement never recalculates entropy: the plot is cached as a native bitmap, and only the old/new cursor strips are invalidated. Duplicate mouse positions do not trigger painting. Selection notifications are coalesced to avoid scanning the whole file between deselection and selection.

Selecting a table row recalculates the entire chart for that section's available raw bytes, or for the overlay. The Full button beside Refresh clears the selection and restores the whole-file map, preserving the block count. Clearing the selection also restores the whole-file map. The chart subtitle identifies the current scope. Block-count changes and Refresh preserve the selected row. Empty raw sections show No data.

Moving the mouse over the plot displays the absolute hexadecimal file offset beneath it and a thin red vertical line across the plot at the pointer's horizontal position. Left-clicking copies that offset alone (for example, `0x00123456`) as Unicode text to the clipboard and shows a copy confirmation. Section and overlay views include their file base offset. Leaving the plot clears the readout and hides the line.

The table ends after its last visible record. Large section lists scroll within the table to preserve room for the entropy map below. The Blocks dropdown beside Refresh offers 16, 32, 64, 128, 256, 512 and 1024 (default 256); selecting a value immediately recalculates the map. The chart covers the entire file in N consecutive intervals with boundaries floor(file_size * i / N), including headers, gaps and overlay. It uses an unlabeled horizontal axis and a fixed entropy scale from 0 to 8. Empty intervals for files smaller than N bytes are left blank. The blue step curve has a gradient area fill and updates with Refresh.

Source-only validation: `check_sources.cmd` compiles the source and resources and runs calculation tests without rebuilding or installing the extension DLL.

Build: run `build.cmd` using the installed Visual Studio C++ tools. It builds the DLL, runs entropy/parser tests, and loads the DLL in a hidden test host to validate exports, API binding, dialog creation, table contents, overlay and Refresh.

Install: run `install.ps1` as administrator. Destination:
`C:\Program Files\NTCore\Explorer Suite\Extensions\CFF Explorer\Entropy\Entropy.dll`

Open/restart CFF Explorer, load a PE file and select the Entropy extension. The installation uses the static C++ runtime, so no additional Visual C++ runtime package is required for this build.

The DLL was tested with a host implementing the documented SDK callbacks. Regression tests cover 10,000 mouse movements/paints and 40 dialog lifecycle cycles, verify cursor pixels and cache invalidation, check that the message queue settles, and track GDI object counts. This does not reproduce every possible CFF Explorer environment; report any remaining host-specific hang with the input file and action.
