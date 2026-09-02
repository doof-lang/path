# std/path Cookbook

This cookbook is the task-oriented companion to the
[API guide](../API.md). It covers common portable path workflows and the
boundary between lexical path handling and filesystem security.

## Normalize and combine path strings

Use `normalize` for one path and `join` when combining parts. Both accept `/`
and native Windows `\` separators, remove `.` segments, resolve `..` without
climbing above an absolute root, and return public paths with `/` separators.

```doof
import { join, normalize } from "std/path"

source := normalize("src/generated/../main.do")
// "src/main.do"

report := join(["build", "reports", "test.json"])
// "build/reports/test.json"

windowsSource := join(["C:\\Users\\doof", "project", "main.do"])
// "C:/Users/doof/project/main.do"
```

These operations are lexical: they do not require paths to exist and do not
resolve symbolic links.

## Resolve command-line paths

Use `absolute` at an application boundary when later code should not depend on
changes to the process working directory. It resolves relative input against
the current working directory and normalizes the result.

```doof
import { absolute } from "std/path"

function resolveInput(argument: string): Result<string, string> {
  return absolute(argument)
}

input := try! resolveInput("documents/report.md")
```

`absolute` does not require the target to exist. Use `std/fs` when existence or
metadata matters; filesystem canonicalization requires a platform-aware API.

## Compute a path between directories

`relative` treats `fromPath` as a directory and returns the path needed to
reach `toPath`. Both inputs are normalized first.

```doof
import { relative } from "std/path"

asset := try! relative(
  "/home/user/project/src/pages",
  "/home/user/project/assets/logo.png",
)
// "../../assets/logo.png"
```

Different roots cannot be related, so `C:/project` to `D:/assets` returns
`Failure`. Windows drive and UNC roots compare case-insensitively.

## Keep an untrusted path inside a directory

Use `resolveWithin` when a relative name comes from an archive, request, or
other untrusted source. The base must be absolute.

```doof
import { resolveWithin } from "std/path"

function uploadDestination(name: string): Result<string, string> {
  return resolveWithin("/srv/example/uploads", name)
}

logo := try! uploadDestination("images/logo.png")
// "/srv/example/uploads/images/logo.png"

blocked := uploadDestination("../../etc/passwd")
// Failure
```

Containment is lexical. Before writing untrusted content, also account for
symbolic links already present beneath the base directory. A link inside the
base can point outside it even when the normalized path remains inside. Avoid
following such links, or validate the filesystem-resolved destination using an
appropriate platform API.

## Load packaged resources

Prefer the resource helpers over paths relative to the working directory.
`resourcePath` uses the packaged application's resource directory and rejects
paths that lexically escape it.

```doof
import { resourcePath } from "std/path"

texturePath := try! resourcePath("images/texture.png")
```

When an API accepts file contents directly, the `std/fs` resource readers are
more concise:

```doof
import { readTextResource } from "std/fs"

template := try! readTextResource("templates/welcome.html")
```

Declare both files and directories in the application's `doof.json`
`resources` list so they are available in development and packaged builds.

## Choose an application directory

Use `dataDirectory` for durable application-owned files and `cacheDirectory`
for replaceable files. Both create the application directory before returning.
Use `tempDirectory` for short-lived process files.

```doof
import { cacheDirectory, dataDirectory, join, tempDirectory } from "std/path"

dataFile := join([try! dataDirectory("dev.example.notes"), "notes.json"])
thumbnail := join([try! cacheDirectory("dev.example.notes"), "thumbs", "42.png"])
scratch := join([tempDirectory(), "dev.example.notes", "import.tmp"])
```

Console applications must supply an application identifier. Packaged
applications can use their bundle identifier when it is omitted.

## Split and change a filename

Combine the splitting helpers with `join` to build a replacement name.

```doof
import { basename, dirname, extension, join, stem } from "std/path"

input := "/srv/reports/archive.tar.gz"
directory := dirname(input)  // "/srv/reports"
filename := basename(input)  // "archive.tar.gz"
name := stem(input)          // "archive.tar"
suffix := extension(input)   // ".gz"
output := join([directory, name + ".zst"])
// "/srv/reports/archive.tar.zst"
```

Only the final suffix is treated as the extension. Leading-dot names such as
`.env` have no extension.

## Treat the working directory as process-global state

`setCurrentWorkingDirectory` changes the whole process, so prefer absolute
paths in libraries and concurrent programs. If a command must change it,
capture the original directory and restore it on every non-terminating path.

```doof
import { currentWorkingDirectory, setCurrentWorkingDirectory } from "std/path"

original := try! currentWorkingDirectory()
try! setCurrentWorkingDirectory("/srv/example")

result := runCommand()
try! setCurrentWorkingDirectory(original)
return result
```

A panic or forced process termination can bypass restoration; avoid changing
the working directory when explicit absolute paths are practical.

Run the path module tests with:

```bash
doof test path
```
