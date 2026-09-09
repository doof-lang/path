# std/path

Portable path manipulation utilities. Public paths use `/` separators on every
platform, while Windows drive roots such as `C:/` and native backslash input are
recognized. String helpers normalize `.` and `..` segments without touching the
filesystem.

`executablePath(): Result<string, string>` returns the normalized absolute path
of the running executable on Windows, macOS, and Linux/GNU. Discovery errors
and unsupported platforms return `Failure`; the path does not imply read access.

## Documentation

- [Guide and API reference](docs/API.md) explains normalization rules, directory helpers, path splitting, and filesystem-independent behavior.
- [Cookbook](docs/cookbook/README.md) shows common workflows for portable paths, resources, relative paths, and lexical containment.
- Tests can be run with `doof test path`.

## Usage

```doof
import {
	absolute, basename, cacheDirectory, currentWorkingDirectory, dataDirectory, dirname, extension, homeDirectory,
	isAbsolute, join, normalize, relative, resolveWithin, resourcePath, resourcesDirectory, setCurrentWorkingDirectory,
	stem, tempDirectory,
} from "std/path"

joined := join(["/home/user", "projects", "../docs/readme.txt"])
// "/home/user/docs/readme.txt"

dir := dirname(joined)    // "/home/user/docs"
base := basename(joined)  // "readme.txt"
name := stem(joined)      // "readme"
ext := extension(joined)  // ".txt"

home := try! homeDirectory()
temp := tempDirectory()
data := try! dataDirectory("dev.example.tool")
cache := try! cacheDirectory("dev.example.tool")
cwd := try! currentWorkingDirectory()
resources := try! resourcesDirectory()
logo := try! resourcePath("images/logo.png")
try! setCurrentWorkingDirectory(home)
resolved := try! absolute("src/main.do")
rel := try! relative("/home/user/projects", "/home/user/docs/readme.txt")
safe := try! resolveWithin("/srv/app", "assets/logo.png")
```

## Exports

#### `absolute(path: string): Result<string, string>`

Resolve a relative path against the current working directory and normalize the result. Already-absolute paths are normalized without changing their root.

```doof
source := try! absolute("src/main.do")
```

---

#### `join(parts: string[]): string`

Join path segments into a single normalized path. Handles `.` (ignored) and `..` (pops the previous segment). An absolute segment in the middle of the list resets all preceding segments.

```doof
join(["foo", "bar", "baz.txt"])           // "foo/bar/baz.txt"
join(["foo/norm/../bar"])                  // "foo/bar"
join(["foo/bar", "/tmp", "logs/out.txt"]) // "/tmp/logs/out.txt"
join(["foo", "../../bar"])                // "../bar"
join([])                                  // "."
```

---

#### `normalize(path: string): string`

Normalize a single path using the same rules as `join`. This is equivalent to `join([path])`.

```doof
normalize("foo//bar/../file.do") // "foo/file.do"
```

---

#### `relative(fromPath: string, toPath: string): Result<string, string>`

Return the normalized path from directory `fromPath` to `toPath`. Returns `Failure` if the paths use different roots, including different Windows drives or network shares.

```doof
relative("/home/user/project/src", "/home/user/assets") // Success("../../assets")
relative("C:/src", "D:/src")                           // Failure
```

---

#### `resolveWithin(base: string, path: string): Result<string, string>`

Resolve `path` lexically within the absolute path `base`. Returns `Failure` if `base` is relative or the result would escape it. This operation does not access the filesystem and therefore does not resolve symbolic links.

```doof
resolveWithin("/srv/app", "assets/logo.png") // Success("/srv/app/assets/logo.png")
resolveWithin("/srv/app", "../secret")       // Failure
```

---

#### `dirname(path: string): string`

Return the directory portion of a path — everything up to (but not including) the last `/`. Returns `"."` if there is no directory component and `"/"` for the root.

```doof
dirname("/home/user/file.txt") // "/home/user"
dirname("file.txt")            // "."
dirname("/")                   // "/"
```

---

#### `basename(path: string): string`

Return the final component of a path (the filename). Returns `""` for the root path.

```doof
basename("/home/user/file.txt") // "file.txt"
basename("file.txt")            // "file.txt"
basename("/")                   // ""
```

---

#### `stem(path: string): string`

Return the filename without its extension. If the name has no extension, or is `"."` / `".."`, the full name is returned.

```doof
stem("/home/user/archive.tar.gz") // "archive.tar"
stem("README.md")                  // "README"
stem("Makefile")                   // "Makefile"
```

---

#### `extension(path: string): string`

Return the extension of the filename, including the leading `.`. Returns `""` if there is no extension.

```doof
extension("archive.tar.gz") // ".gz"
extension("README.md")       // ".md"
extension("Makefile")        // ""
```

---

#### `isAbsolute(path: string): bool`

Return `true` if the path starts with `/` or has a Windows drive root.

```doof
isAbsolute("/usr/local/bin") // true
isAbsolute("C:\\Users\\doof") // true
isAbsolute("relative/path") // false
```

---

#### `homeDirectory(): Result<string, string>`

Return the current user's home directory as an absolute normalized path.

```doof
home := try! homeDirectory()
```

---

#### `tempDirectory(): string`

Return the process temp directory as an absolute normalized path. This uses `TMPDIR` when it is set and falls back to `"/tmp"`.

```doof
tempDirectory() // e.g. "/tmp"
```

---

#### `dataDirectory(appId: string | none = none): Result<string, string>`

Return the per-application data directory as an absolute normalized path. On success, the directory exists and is ready to use; if the target path exists but is not a directory, this returns `Failure`. Bundled applications use their bundle identifier when `appId` is omitted, and reject a supplied `appId` unless it matches the bundle identifier. Console applications require `appId`.

```doof
data := try! dataDirectory("dev.example.tool")
```

---

#### `cacheDirectory(appId: string | none = none): Result<string, string>`

Return the per-application cache directory as an absolute normalized path. On success, the directory exists and is ready to use; if the target path exists but is not a directory, this returns `Failure`. Bundled applications use their bundle identifier when `appId` is omitted, and reject a supplied `appId` unless it matches the bundle identifier. Console applications require `appId`.

```doof
cache := try! cacheDirectory("dev.example.tool")
```

---

#### `currentWorkingDirectory(): Result<string, string>`

Return the process current working directory as an absolute normalized path.

```doof
cwd := try! currentWorkingDirectory()
```

---

#### `resourcesDirectory(): Result<string, string>`

Return the directory that should be used to load bundled application resources. For macOS `.app` bundles this resolves to `Contents/Resources`; for iOS `.app` bundles this resolves to the app bundle directory. Other builds resolve to the directory containing the executable.

```doof
resources := try! resourcesDirectory()
```

---

#### `resourcePath(path: string): Result<string, string>`

Resolve `path` against `resourcesDirectory()` and return the normalized absolute path. Returns `Failure` if the normalized result would escape the resources directory.

```doof
logo := try! resourcePath("images/logo.png")
blocked := resourcePath("../../badpanda") // Failure
```

---

#### `setCurrentWorkingDirectory(path: string): Result<none, string>`

Change the process current working directory.

```doof
try! setCurrentWorkingDirectory("/tmp")
```
