import {
  basename, currentWorkingDirectory, dirname, extension, homeDirectory,
  isAbsolute, join, setCurrentWorkingDirectory, stem, tempDirectory,
} from "./index"

function isSuccess<T, E>(result: Result<T, E>): bool {
  return case result {
    _: Success => true,
    _: Failure => false
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