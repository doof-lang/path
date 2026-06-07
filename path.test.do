import {
  basename, cacheDirectory, currentWorkingDirectory, dataDirectory, dirname, extension, homeDirectory,
  isAbsolute, join, resourcesDirectory, setCurrentWorkingDirectory, stem, tempDirectory,
} from "./index"
import { isDirectory, remove, writeText } from "std/fs"

function isSuccess<T, E>(result: Result<T, E>): bool {
  return case result {
    _: Success -> true,
    _: Failure -> false
  }
}

function isFailure<T, E>(result: Result<T, E>): bool {
  return case result {
    _: Success -> false,
    _: Failure -> true
  }
}

function matchesWorkingDirectoryPath(actual: string, expected: string): bool {
  normalizedActual := join([actual])
  normalizedExpected := join([expected])

  if normalizedActual == normalizedExpected {
    return true
  }

  // macOS commonly exposes /var through a /private/var realpath in getcwd().
  return normalizedActual == "/private" + normalizedExpected
    || normalizedExpected == "/private" + normalizedActual
}

export function testJoinConcatenatesRelativeParts(): void {
  assert(
    join(["foo", "bar", "baz.txt"]) == "foo/bar/baz.txt",
    "expected join to concatenate relative path parts"
  )
}

export function testJoinNormalizesSinglePart(): void {
  assert(
    join(["foo/norm/../bar"]) == "foo/bar",
    "expected join to normalize a single path argument"
  )
}

export function testJoinResetsWhenANewAbsolutePartAppears(): void {
  assert(
    join(["foo/bar", "/tmp", "logs/output.txt"]) == "/tmp/logs/output.txt",
    "expected an absolute part to override earlier path segments"
  )
}

export function testJoinPreservesRelativeParentTraversal(): void {
  assert(
    join(["foo", "../../bar"]) == "../bar",
    "expected join to preserve leading parent traversal in relative paths"
  )
}

export function testJoinClampsParentTraversalAtRoot(): void {
  assert(
    join(["/foo", "../../bar"]) == "/bar",
    "expected absolute joins to stop parent traversal at the root"
  )
}

export function testJoinReturnsDotForAnEmptyRelativeResult(): void {
  assert(
    join(["foo", ".."]) == ".",
    "expected join to use dot for an empty relative path"
  )
  assert(join([]) == ".", "expected join of no parts to be dot")
}

export function testDirnameAndBasenameNormalizeTrailingSeparators(): void {
  assert(dirname("/foo/bar/") == "/foo", "expected dirname to normalize trailing separators")
  assert(basename("/foo/bar/") == "bar", "expected basename to normalize trailing separators")
  assert(dirname("single") == ".", "expected dirname of a single relative name to be dot")
}

export function testStemAndExtensionSplitTheFinalSuffix(): void {
  assert(stem("/foo/archive.tar.gz") == "archive.tar", "expected stem to drop only the final suffix")
  assert(extension("/foo/archive.tar.gz") == ".gz", "expected extension to return only the final suffix")
}

export function testStemAndExtensionTreatLeadingDotsAsPartOfTheName(): void {
  assert(stem(".gitignore") == ".gitignore", "expected a leading dot file to keep its full stem")
  assert(extension(".gitignore") == "", "expected a leading dot file to have no extension")
}

export function testIsAbsoluteChecksForLeadingSlash(): void {
  assert(isAbsolute("/tmp/log") == true, "expected a leading slash path to be absolute")
  assert(isAbsolute("tmp/log") == false, "expected a relative path to remain relative")
}

export function testHomeAndTempDirectoryReturnAbsolutePaths(): void {
  home := try! homeDirectory()
  temp := tempDirectory()

  assert(home.length > 0, "expected homeDirectory to return a non-empty path")
  assert(isAbsolute(home), "expected homeDirectory to return an absolute path")
  assert(temp.length > 0, "expected tempDirectory to return a non-empty path")
  assert(isAbsolute(temp), "expected tempDirectory to return an absolute path")
}

export function testResourcesDirectoryReturnsAnAbsolutePath(): void {
  resources := try! resourcesDirectory()

  assert(resources.length > 0, "expected resourcesDirectory to return a non-empty path")
  assert(isAbsolute(resources), "expected resourcesDirectory to return an absolute path")
}

export function testApplicationDirectoriesRequireAnIdentifierForConsoleApps(): void {
  data := dataDirectory()
  cache := cacheDirectory()

  assert(isFailure(data), "expected dataDirectory without an app id to fail for console applications")
  assert(isFailure(cache), "expected cacheDirectory without an app id to fail for console applications")
}

export function testApplicationDirectoriesUseSuppliedIdentifierForConsoleApps(): void {
  appId := "dev.doof.path-tests"
  data := try! dataDirectory(appId)
  cache := try! cacheDirectory(appId)

  assert(data.length > 0, "expected dataDirectory to return a non-empty path")
  assert(cache.length > 0, "expected cacheDirectory to return a non-empty path")
  assert(isAbsolute(data), "expected dataDirectory to return an absolute path")
  assert(isAbsolute(cache), "expected cacheDirectory to return an absolute path")
  assert(basename(data) == appId, "expected dataDirectory to use the supplied app id")
  assert(basename(cache) == appId, "expected cacheDirectory to use the supplied app id")
}

export function testApplicationDirectoriesAreCreatedAndReadyToUse(): void {
  dataAppId := "dev.doof.path-tests-created-data"
  cacheAppId := "dev.doof.path-tests-created-cache"
  data := try! dataDirectory(dataAppId)
  cache := try! cacheDirectory(cacheAppId)
  dataProbe := join([data, "probe.txt"])
  cacheProbe := join([cache, "probe.txt"])

  assert(isDirectory(data), "expected dataDirectory to create a directory")
  assert(isDirectory(cache), "expected cacheDirectory to create a directory")

  try! writeText(dataProbe, "data")
  try! writeText(cacheProbe, "cache")

  try! remove(dataProbe)
  try! remove(cacheProbe)
  try! remove(data)
  try! remove(cache)
}

export function testApplicationDirectoryFailsWhenTargetIsNotADirectory(): void {
  appId := "dev.doof.path-tests-file-conflict"
  directory := try! cacheDirectory(appId)

  try! remove(directory)
  try! writeText(directory, "not a directory")

  blocked := cacheDirectory(appId)
  assert(isFailure(blocked), "expected cacheDirectory to fail when the target path is a file")

  try! remove(directory)
}

export function testCurrentWorkingDirectoryAndSetterRoundTrip(): void {
  original := try! currentWorkingDirectory()
  let target = tempDirectory()
  if target == original {
    target = try! homeDirectory()
  }

  changeResult := setCurrentWorkingDirectory(target)
  assert(isSuccess(changeResult), "expected setCurrentWorkingDirectory to succeed for a known directory")

  changed := try! currentWorkingDirectory()

  restoreResult := setCurrentWorkingDirectory(original)
  assert(isSuccess(restoreResult), "expected to restore the original working directory")

  restored := try! currentWorkingDirectory()

  assert(matchesWorkingDirectoryPath(changed, target), "expected currentWorkingDirectory to reflect the changed directory")
  assert(restored == join([original]), "expected currentWorkingDirectory to match the restored directory")
}

export function testEmptyJoin() {
  assert(join([]) == ".", "expected empty join to resolve to dot")
  assert(join([""]) == ".", "expected empty string join to resolve to dot")
  assert(join(["foo", "", "bar"]) == "foo/bar", "expected empty string in join to be ignored")
}

export function testEmptyExtension() {
    assert(extension("foo/bar") == "", "Paths without extensions has a blank extension")
}

export function testTopDirName() {
    assert(dirname("/bar") == "/", "Root is dirname of top level dir")
}
