# std/path

POSIX path manipulation utilities. All functions operate on string paths and handle normalization (resolving `.` and `..` segments) without touching the filesystem.

## Usage

```doof
import { join, dirname, basename, stem, extension, isAbsolute } from "std/path"

joined := join(["/home/user", "projects", "../docs/readme.txt"])
// "/home/user/docs/readme.txt"

dir := dirname(joined)    // "/home/user/docs"
base := basename(joined)  // "readme.txt"
name := stem(joined)      // "readme"
ext := extension(joined)  // ".txt"
```

## Exports

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

Return `true` if the path starts with `/`.

```doof
isAbsolute("/usr/local/bin") // true
isAbsolute("relative/path") // false
```
