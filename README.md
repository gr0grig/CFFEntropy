# Entropy for CFF Explorer

A native Windows x64 plugin for [CFF Explorer](https://ntcore.com/explorer-suite/) that displays the Shannon entropy of PE files, their sections, and any overlay data. It helps you inspect how byte distributions vary across a file using a section table and an interactive entropy chart.

The plugin reads the current document without modifying it. Entropy ranges from 0 to 8 bits per byte: higher values indicate a more uniform byte distribution, which can occur in compressed or encrypted data. Entropy alone does not identify the contents or purpose of a file.

## Features

- Section and overlay table with file offsets, raw sizes, percentage of the file, and entropy values.
- Entropy chart for the whole file, an individual section, or the overlay.
- Adjustable chart resolution from 16 to 1024 blocks.
- File offset readout under the mouse cursor; click the chart to copy the offset.
- Refresh support for inspecting the current document after edits in CFF Explorer.

## Requirements

- Windows x64.
- The **64-bit version of CFF Explorer**, included in [Explorer Suite from NTCore](https://ntcore.com/explorer-suite/).

The bundled `Entropy.dll` is an x64 build and cannot be loaded by 32-bit CFF Explorer. No separate Visual C++ runtime installation is required for this build.

## Installation

1. Download and install Explorer Suite from the [official CFF Explorer page](https://ntcore.com/explorer-suite/).
2. Close CFF Explorer if it is running.
3. Download [`Entropy.dll`](Entropy.dll) from the root of this repository. On GitHub, open the file and choose **Download raw file**.
4. Inside your Explorer Suite installation directory, create the folder `Extensions\CFF Explorer\Entropy` if it does not already exist.
5. Copy `Entropy.dll` into that folder. For a default installation, the resulting path is:

   ```text
   C:\Program Files\NTCore\Explorer Suite\Extensions\CFF Explorer\Entropy\Entropy.dll
   ```

   If Explorer Suite is installed elsewhere, use the same relative folder structure under its installation directory. Copying into `Program Files` may require administrator permission.

6. Start **64-bit CFF Explorer**, open a PE file such as an EXE or DLL, and select the **Entropy** extension.

To update the plugin, close CFF Explorer, replace the installed `Entropy.dll`, and reopen the application. To uninstall it, close CFF Explorer and remove the installed DLL.

## Usage

The initial chart covers the whole file. Select a row in the table to inspect a section or the overlay, and click **Full** to return to the whole-file view. Use **Blocks** to change the chart resolution and **Refresh** after editing the document.

Move the mouse over the chart to see an absolute hexadecimal file offset. Click to copy that offset to the clipboard.

If the extension does not appear, check that you are running 64-bit CFF Explorer, verify the installation path above, and restart CFF Explorer.

## Source code and building

Plugin source, tests, and build scripts are in [`Entropy/`](Entropy/). The required CFF Explorer SDK headers are included in [`sdk/`](sdk/).

With the Visual Studio C++ tools and Windows SDK installed, run:

```bat
cd Entropy
build.cmd
```

The build script currently uses Visual Studio 2026 Community at its default installation path; adjust the `vcvars64.bat` path in the script if your installation differs. It produces `Entropy\build\Entropy.dll` and runs the included tests. Install that DLL using the steps above, or run `Entropy\install.ps1` as administrator after a successful build to install it to the default Explorer Suite directory.

See [`Entropy/README.md`](Entropy/README.md) for calculation details and additional validation commands.
