# Atom Packager

Packager reads and writes APKG v2 archives containing files, a directory tree, and asset metadata.
The current format implementation stores uncompressed chunks. APKG v1 has been retired.

## Command line and interactive terminal

```text
packager_tool pack <input> <output.apkg> [package_path]
packager_tool list <package.apkg>
packager_tool verify <package.apkg>
packager_tool --tui
packager_tool --path <package.apkg>
```

For a directory, the optional package path is a prefix. For a single file, supply the complete path
inside the archive. Package paths use forward slashes and follow VFS path rules. The scripted `pack`
command retains its overwrite behavior and exit codes. The interactive terminal asks before replacing
an existing output.

## Desktop app

Run `packager_tool` to open the GUI, or use `packager_tool --open example.apkg` to load an archive.
Use the same executable for terminal commands. `--path` starts the TUI with an archive already open.
On Windows 11 24H2 and later, launching the standard Release build from Explorer opens the GUI without a console.
Browse shows the directory tree, path
search, kind filters, resource metadata, and verification. Build picks a file or directory and lets
you set the output and package path. Build and verify run on a worker, and Activity shows the
Atom_Log Packager channel. The app works with the engine debugger disabled.
The inspector previews images, validates JSON/JSONC, displays Lua source as text, and plays supported audio on demand.

## Engine API

- `PackageBuilder::AddFile`, `AddDirectory`, and `Build` create an archive.
- `PackageFileSystem::Create` validates the archive structure. Use `EnumerateAssets` or
  `EnumerateDirectories` to inspect the index, and `VerifyContents` to check chunks and whole assets.
- `Vfs::MountPackage` mounts an archive for streaming through `IFile`. Remount after replacing an
  archive at the same path. New handles reject a replacement with a different header.
