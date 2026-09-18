![](E:/xToolsMenu/ICON.png)

# xToolsMenu

xToolsMenu is an advanced Windows Shell Extension that adds a customizable "Power User" context menu to Windows Explorer. It provides quick access to common administrative, navigation, and development tasks directly from your right-click menu, engineered specifically to maximize efficiency while completely eliminating workspace clutter.

## Dual-Version Menu Support

Windows 11 introduced a streamlined, modern context menu while keeping the classic menu accessible under "Show more options". Unlike basic extensions, **xToolsMenu supports both versions simultaneously** through a dual-architecture implementation:

- **Modern Menu (Windows 11 Native)**: Implemented via a COM-based `IExplorerCommand` server and registered as a Windows Sparse Package (`AppxManifest.xml`). It integrates natively and seamlessly into the primary Windows 11 right-click interface.
- **Classic Menu (Legacy / Windows 10 Style)**: Implemented as a classic shell extension (`IContextMenu`) via standard system registry registration. It appears when using older Windows versions or when selecting "Show more options" in Windows 11.

## Advanced Context-Aware Filtering & Clutter Reduction

To prevent the context menu from becoming bloated and disorganized, xToolsMenu features a **highly intelligent, dynamic filtering engine**. Commands are not just disabled or grayed out; they are entirely hidden based on the precise target context, ensuring a **zero-clutter user experience**.

### 1. Target Location Filtering

The extension detects exactly where the right-click occurred and adapts the menu dynamically:

Command
Files Target
Directory Target (Folder)
Directory Background (Empty Space)

**Attributes**
✅ Visible
✅ Visible
✅ Visible

**Copy Name / Path**
✅ Visible
✅ Visible
❌ *Hidden*

**Edit With**
✅ Visible
❌ *Hidden*
❌ *Hidden*

**Take Ownership**
✅ Visible
✅ Visible
❌ *Hidden*

**Terminal / Terminal (Admin)**
❌ *Hidden*
✅ Visible
✅ Visible

**System Folders**
❌ *Hidden*
❌ *Hidden*
✅ Visible

**Paste to File**
❌ *Hidden*
❌ *Hidden*
✅ Visible

- **No Confusing Options**: You will never see folder-centric options like *System Folders* or *Paste to File* when clicking a single image file, nor will you see file-specific tools like *Edit With* when clicking the desktop background.

### 2. Independent Dual-Menu Customization

Through the configuration app (`Settings.exe`), you can toggle every single feature **independently for both the Modern and Classic menus**.

> 
> [!TIP]
> **Optimized Workflow Strategy:** Keep your primary Windows 11 modern context menu ultra-minimal with only your top 2 or 3 daily tools to maintain maximum aesthetic cleanliness. Then, enable the full "kitchen sink" powerhouse suite on the Classic menu, keeping advanced operations exactly one click away under "Show more options".
> 
> 
> 

### 3. Granular Extension & File Type Filtering

When extending xToolsMenu with custom user scripts or tools, the filtering engine goes even deeper. Custom commands can be restricted to appear only for specific file format categories:

- **Supported Categories**: `Images`, `Text Files`, `Documents`, `Audio`, `Video`, `Archives`, `Disk Images`, `Executables`.
- **How it works**: A custom Python execution shortcut or code minifier command can be configured to target *only* `Text Files`. When right-clicking a `.mp4` or a `.zip` archive, that command automatically vanishes, keeping your environment perfectly clean and relevant to the file at hand.

## Features

- **Attributes**: Quickly view and modify file/folder attributes and timestamps via a lightweight, dedicated dark-mode dialog (`AttributesDialog.exe`).
- **Terminal & Admin Terminal**: Launch a Command Prompt (`cmd.exe`) or elevated shell instantly, automatically initialized to the active folder path.
- **Edit with**: Quickly route selected files into specific developer editors using `EditWithDialog.exe`.
- **System Folders**: Rapid navigation panel providing quick jumps to hidden or deeply nested system folders (`SystemFoldersDialog.exe`).
- **Paste to File**: Creates a `Clipboard.txt` file in the current directory containing whatever text is currently stored in your system clipboard—perfect for quick note extraction.
- **Copy Name / Copy Path**: Instantly copies filenames or absolute file paths of all selected elements directly to your clipboard.
- **Take Ownership**: Recursively takes ownership of restricted files/folders, granting full control permissions to the current user and the Administrators group in a single click.

## Extensions & Custom Commands

xToolsMenu is fully expandable. You can build and import custom extensions to run automated tools or scripts with personalized arguments, custom icons, and specialized visibility rules.

- **Extensions Gist & Templates:** [https://gist.github.com/xCONFLiCTiONx/a1c0edb7bab912ca5012985308ea9e34](https://gist.github.com/xCONFLiCTiONx/a1c0edb7bab912ca5012985308ea9e34)

## Implementation Details

### Core Architecture

The extension is written in native C++ for zero-latency performance and high reliability:

- **Root Menu**: `XToolsMenuCommand` implements the modern shell entry point and hosts subcommands.
- **Classic Handler**: Traditional COM implementation supporting classic `IContextMenu` interfaces for backward compatibility.
- **Smart Enumeration**: `XToolsCommandEnumerator` handles the runtime list composition and runs file category classification checks on-demand.