import { basename, dirname, extension, isAbsolute, join, stem } from "./index"

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