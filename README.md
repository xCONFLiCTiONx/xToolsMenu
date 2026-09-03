![xToolsMenu Logo](ICON.png)

# xToolsMenu

xToolsMenu is a Windows 11 Shell Extension that adds a "Power User" context menu to Windows Explorer. It provides quick access to common administrative and development tasks directly from the right-click menu.

## Features

- **Attributes**: Quickly view and modify file/folder attributes via a dedicated dialog (`AttributesDialog.exe`).
- **Terminal**: Open a command prompt (`cmd.exe`) in the selected directory or the background directory. (Hidden when selecting a file).
- **Edit with**: Open files in a specific editor via `EditWithDialog.exe`. (Automatically hidden for folders and directory background).
- **System Folders**: Quick navigation to common Windows system folders (`SystemFoldersDialog.exe`). (Available only when right-clicking the directory background).
- **Paste to File**: Creates a `Clipboard.txt` file in the current directory containing the text currently in your clipboard. (Available only when right-clicking the directory background).
- **Copy Name**: Copies the filenames of all selected items to the clipboard. (Hidden when clicking directory background).
- **Copy Path**: Copies the full absolute paths of all selected items to the clipboard. (Hidden when clicking directory background).
- **Take Ownership**: Recursively takes ownership of the selected files or folders and grants full control to the current user and the Administrators group.

## Implementation Details

### Core Architecture

The application is implemented as a COM-based Shell Extension using the `IExplorerCommand` interface.

- **Root Menu**: `XToolsMenuCommand` implements the main entry point and hosts the subcommands.
- **Sub-commands**: `XToolsSubCommand` handles individual actions, icons, and visibility states.
- **Enumeration**: `XToolsCommandEnumerator` manages the list of available commands.

### Key Logic

- **Dynamic Visibility**: The `GetState` method determines which menu items are visible based on the selection (e.g., hiding file-only tools when a folder is selected).
- **Icon Loading**: The extension dynamically searches for an `ICON.ico` in its installation directory to use as the menu icon.
- **Elevation**: The "Take Ownership" feature uses `ShellExecuteEx` with the `runas` verb to perform administrative tasks via `takeown.exe` and `icacls.exe`.
- **Clipboard Integration**: Uses the Windows Clipboard API (`OpenClipboard`, `SetClipboardData`, etc.) to handle copy/paste operations natively.

## Project Structure

- `src/ShellExtension.cpp`: Main implementation of the Shell Extension logic.
- `src/ShellExtension.h`: Class definitions and action enums.
- `AttributesDialog.exe`, `EditWithDialog.exe`, `SystemFoldersDialog.exe`: Helper applications called by the extension.
- `ICON.ico`: The application icon.

## Installation

The extension is a DLL that must be registered with Windows using `regsvr32` or a custom installer that sets the appropriate registry keys under `HKEY_CLASSES_ROOT\CLSID` and associations in `DesktopBackground`, `Drive`, `Directory`, and `*`.