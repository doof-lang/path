# std/path Guide

`std/path` provides portable path manipulation and application directory
helpers. Public paths use `/` separators on every platform. String operations
such as `join`, `dirname`, and `basename` do not touch the filesystem. Directory
discovery helpers call native platform APIs and return normalized absolute paths.
Windows drive roots use the form `C:/`; native backslash input is accepted.

## Normalization Rules

`join(parts)` is the central normalization helper:

- empty parts are ignored
- `.` segments are ignored
- `..` removes the previous normal segment
- leading `..` is preserved for relative paths
- `..` cannot climb above the root of an absolute path
- an absolute part resets all earlier parts
- an empty relative result renders as `.`

```doof
join(["foo", "bar", "baz.txt"])          // "foo/bar/baz.txt"
join(["foo/norm/../bar"])                // "foo/bar"
join(["foo/bar", "/tmp", "out.txt"])     // "/tmp/out.txt"
join(["foo", "../../bar"])               // "../bar"
join([])                                 // "."
```

All native directory helpers normalize their successful results through
`join([path])`, so callers get consistent path strings.

`absolute(path)` resolves relative input against the current working directory
and applies the same normalization. It returns `Failure` if the working
directory cannot be read.

`normalize(path)` is the named single-path form of `join([path])`.

`relative(fromPath, toPath)` returns the normalized path from directory
`fromPath` to `toPath`. It returns `Failure` when the paths have different roots.

`resolveWithin(base, path)` resolves a path beneath an absolute base and
returns `Failure` if lexical normalization would escape that base. It does not
access the filesystem, so applications must separately account for symbolic
links when containment depends on the filesystem layout.

## Path Splitting

Use these helpers for string-level path analysis:

- `dirname(path)` returns the directory portion, `.` for plain filenames, and `/` for root.
- `basename(path)` returns the last path component, or `""` for root.
- `stem(path)` returns the basename without the final extension.
- `extension(path)` returns the final extension including the leading dot, or `""`.
- `isAbsolute(path)` recognizes a leading `/` or a Windows drive root such as `C:/`.

Leading dots are treated as part of the filename, so `.env` has no extension.

## Application Directories

`dataDirectory(appId)` and `cacheDirectory(appId)` return per-application
directories and ensure the returned directory exists. Console applications
require an `appId`. Bundled applications can use their bundle identifier when
`appId` is omitted.

If the target exists but is not a directory, the result is `Failure<string>`.

## Resource Paths

`resourcesDirectory()` returns the base directory for bundled resources.
`resourcePath(path)` resolves a resource-relative path against that base and
returns `Failure` if normalization would escape the resources directory.

```doof
logo := try! resourcePath("images/logo.png")
blocked := resourcePath("../../badpanda") // Failure
```

## API

### Native-backed directory helpers

```doof
export function homeDirectory(): Result<string, string>
export function tempDirectory(): string
export function dataDirectory(appId: string | none = none): Result<string, string>
export function cacheDirectory(appId: string | none = none): Result<string, string>
export function currentWorkingDirectory(): Result<string, string>
export function absolute(path: string): Result<string, string>
export function resourcesDirectory(): Result<string, string>
export function resourcePath(path: string): Result<string, string>
export import function setCurrentWorkingDirectory(path: string): Result<none, string>
```

### String path helpers

```doof
export function join(parts: string[]): string
export function normalize(path: string): string
export function relative(fromPath: string, toPath: string): Result<string, string>
export function resolveWithin(base: string, path: string): Result<string, string>
export function dirname(path: string): string
export function basename(path: string): string
export function stem(path: string): string
export function extension(path: string): string
export function isAbsolute(path: string): bool
```

All declarations are defined in [index.do](../index.do).
