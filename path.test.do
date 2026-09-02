import {
  absolute, basename, cacheDirectory, currentWorkingDirectory, dataDirectory, dirname, extension, homeDirectory,
  isAbsolute, join, normalize, relative, resolveWithin, resourcePath, resourcesDirectory, setCurrentWorkingDirectory,
  stem, tempDirectory,
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

export function testAbsoluteResolvesRelativeAndNormalizesAbsolutePaths(): none {
  cwd := try! currentWorkingDirectory()
  assert(try! absolute(".") == join([cwd]), "expected dot to resolve to the working directory")
  assert(try! absolute("nested/../file.do") == join([cwd, "file.do"]), "expected relative paths to resolve and normalize")
  assert(try! absolute("/tmp/../tmp/file.do") == "/tmp/file.do", "expected absolute paths to remain absolute and normalize")
}

export function testAbsoluteNormalizesNativeWindowsSeparators(): none {
  assert(try! absolute("\\rooted\\folder") == try! absolute("/rooted/folder"), "expected rooted backslash input to resolve from the filesystem root")
  assert(try! absolute("\\\\server\\share\\folder") == "//server/share/folder", "expected a UNC path to remain absolute")
}

export function testJoinConcatenatesRelativeParts(): none {
  assert(
    join(["foo", "bar", "baz.txt"]) == "foo/bar/baz.txt",
    "expected join to concatenate relative path parts"
  )
}

export function testJoinNormalizesSinglePart(): none {
  assert(
    join(["foo/norm/../bar"]) == "foo/bar",
    "expected join to normalize a single path argument"
  )
}

export function testJoinResetsWhenANewAbsolutePartAppears(): none {
  assert(
    join(["foo/bar", "/tmp", "logs/output.txt"]) == "/tmp/logs/output.txt",
    "expected an absolute part to override earlier path segments"
  )
}

export function testJoinPreservesRelativeParentTraversal(): none {
  assert(
    join(["foo", "../../bar"]) == "../bar",
    "expected join to preserve leading parent traversal in relative paths"
  )
}

export function testJoinClampsParentTraversalAtRoot(): none {
  assert(
    join(["/foo", "../../bar"]) == "/bar",
    "expected absolute joins to stop parent traversal at the root"
  )
}

export function testJoinReturnsDotForAnEmptyRelativeResult(): none {
  assert(
    join(["foo", ".."]) == ".",
    "expected join to use dot for an empty relative path"
  )
  assert(join([]) == ".", "expected join of no parts to be dot")
}

export function testNormalizeProvidesNamedSinglePathNormalization(): none {
  assert(normalize("foo//bar/../file.do") == "foo/file.do", "expected normalize to clean a relative path")
  assert(normalize("C:\\Users\\doof\\..\\main.do") == "C:/Users/main.do", "expected normalize to use public separators")
  assert(normalize("") == ".", "expected normalize of an empty path to be dot")
}

export function testRelativeBuildsPathsBetweenDirectories(): none {
  assert(try! relative("/home/user/project/src", "/home/user/assets/logo.png") == "../../assets/logo.png", "expected a relative path between sibling trees")
  assert(try! relative("/home/user/project", "/home/user/project") == ".", "expected identical paths to produce dot")
  assert(try! relative("foo/bar/..", "foo/baz/./file.do") == "baz/file.do", "expected relative to normalize both inputs")
}

export function testRelativeHandlesWindowsAndNetworkRoots(): none {
  assert(try! relative("C:/Users/doof/src", "c:/Users/doof/tests") == "../tests", "expected drive roots to compare without case")
  assert(try! relative("C:/Users/Doof", "c:/users/doof/tests") == "tests", "expected Windows path segments to compare without case")
  assert(try! relative("//server/share/src", "//SERVER/SHARE/assets") == "../assets", "expected network roots to compare without case")
  assert(isFailure(relative("C:/src", "D:/src")), "expected different drive roots to fail")
  assert(isFailure(relative("//server/share/src", "//server/other/src")), "expected different network shares to fail")
  assert(isFailure(relative("relative", "/absolute")), "expected mixed relative and absolute paths to fail")
}

export function testResolveWithinAcceptsPathsInsideAnAbsoluteBase(): none {
  assert(try! resolveWithin("/srv/app", "assets/logo.png") == "/srv/app/assets/logo.png", "expected a child path to resolve")
  assert(try! resolveWithin("/srv/app", "assets/../config.json") == "/srv/app/config.json", "expected safe traversal to normalize")
  assert(try! resolveWithin("/srv/app", ".") == "/srv/app", "expected the base itself to resolve")
  assert(try! resolveWithin("/", "tmp/file.do") == "/tmp/file.do", "expected the filesystem root to contain absolute children")
  assert(try! resolveWithin("C:/Users/Doof", "c:/users/doof/file.do") == "c:/users/doof/file.do", "expected Windows containment to compare without case")
}

export function testResolveWithinRejectsPathsOutsideAnAbsoluteBase(): none {
  assert(isFailure(resolveWithin("/srv/app", "../secret")), "expected parent traversal outside the base to fail")
  assert(isFailure(resolveWithin("/srv/app", "/srv/application/file")), "expected a sibling with the same text prefix to fail")
  assert(isFailure(resolveWithin("C:/app", "D:/file")), "expected a different drive root to fail")
  assert(isFailure(resolveWithin("relative/base", "file")), "expected a relative base to fail")
}

export function testDirnameAndBasenameNormalizeTrailingSeparators(): none {
  assert(dirname("/foo/bar/") == "/foo", "expected dirname to normalize trailing separators")
  assert(basename("/foo/bar/") == "bar", "expected basename to normalize trailing separators")
  assert(dirname("single") == ".", "expected dirname of a single relative name to be dot")
}

export function testStemAndExtensionSplitTheFinalSuffix(): none {
  assert(stem("/foo/archive.tar.gz") == "archive.tar", "expected stem to drop only the final suffix")
  assert(extension("/foo/archive.tar.gz") == ".gz", "expected extension to return only the final suffix")
}

export function testStemAndExtensionTreatLeadingDotsAsPartOfTheName(): none {
  assert(stem(".gitignore") == ".gitignore", "expected a leading dot file to keep its full stem")
  assert(extension(".gitignore") == "", "expected a leading dot file to have no extension")
}

export function testIsAbsoluteChecksForLeadingSlash(): none {
  assert(isAbsolute("/tmp/log") == true, "expected a leading slash path to be absolute")
  assert(isAbsolute("tmp/log") == false, "expected a relative path to remain relative")
}

export function testWindowsDrivePathsAreAbsoluteAndNormalized(): none {
  assert(isAbsolute("C:/Users/doof") == true, "expected a slash-separated Windows drive path to be absolute")
  assert(isAbsolute("C:\\Users\\doof") == true, "expected a backslash-separated Windows drive path to be absolute")
  assert(isAbsolute("C:relative") == false, "expected a drive-relative Windows path to remain relative")
  assert(isAbsolute("1:/not-a-drive") == false, "expected a non-letter drive prefix to remain relative")
  assert(join(["C:\\Users\\doof", "src", "..", "main.do"]) == "C:/Users/doof/main.do", "expected Windows paths to normalize to public slash separators")
}

export function testWindowsDriveRootPathHelpers(): none {
  assert(join(["C:/"]) == "C:/", "expected join to preserve a Windows drive root")
  assert(dirname("C:/file.do") == "C:/", "expected dirname to preserve a Windows drive root")
  assert(basename("C:/") == "", "expected basename of a Windows drive root to be empty")
}

export function testWindowsNetworkPathsPreserveTheirShareRoot(): none {
  assert(isAbsolute("\\\\server\\share\\folder"), "expected a Windows network path to be absolute")
  assert(join(["\\\\server\\share\\folder", "..", "file.do"]) == "//server/share/file.do", "expected a Windows network path to preserve its share root")
  assert(join(["//server/share", "..", ".."]) == "//server/share/", "expected parent traversal to stop at a Windows share root")
}

export function testHomeAndTempDirectoryReturnAbsolutePaths(): none {
  home := try! homeDirectory()
  temp := tempDirectory()

  assert(home.length > 0, "expected homeDirectory to return a non-empty path")
  assert(isAbsolute(home), "expected homeDirectory to return an absolute path")
  assert(temp.length > 0, "expected tempDirectory to return a non-empty path")
  assert(isAbsolute(temp), "expected tempDirectory to return an absolute path")
}

export function testResourcesDirectoryReturnsAnAbsolutePath(): none {
  resources := try! resourcesDirectory()

  assert(resources.length > 0, "expected resourcesDirectory to return a non-empty path")
  assert(isAbsolute(resources), "expected resourcesDirectory to return an absolute path")
}

export function testResourcePathResolvesInsideResourcesDirectory(): none {
  resources := try! resourcesDirectory()
  resolved := try! resourcePath("images/logo.png")

  assert(resolved == join([resources, "images/logo.png"]), "expected resourcePath to resolve relative resources")
}

export function testResourcePathNormalizesTraversalInsideResourcesDirectory(): none {
  resources := try! resourcesDirectory()
  resolved := try! resourcePath("images/../config.json")

  assert(resolved == join([resources, "config.json"]), "expected resourcePath to normalize safe traversal")
}

export function testResourcePathRejectsParentTraversalOutsideResourcesDirectory(): none {
  blocked := resourcePath("../../badpanda")

  assert(isFailure(blocked), "expected resourcePath to reject paths escaping the resources directory")
}

export function testResourcePathRejectsAbsolutePathOutsideResourcesDirectory(): none {
  blocked := resourcePath("/tmp/badpanda")

  assert(isFailure(blocked), "expected resourcePath to reject absolute paths outside the resources directory")
}

export function testApplicationDirectoriesRequireAnIdentifierForConsoleApps(): none {
  data := dataDirectory()
  cache := cacheDirectory()

  assert(isFailure(data), "expected dataDirectory without an app id to fail for console applications")
  assert(isFailure(cache), "expected cacheDirectory without an app id to fail for console applications")
}

export function testApplicationDirectoriesRejectNativePathSeparators(): none {
  assert(isFailure(dataDirectory("dev/doof")), "expected a slash in an application id to be rejected")
  assert(isFailure(cacheDirectory("dev\\doof")), "expected a backslash in an application id to be rejected")
}

export function testApplicationDirectoriesUseSuppliedIdentifierForConsoleApps(): none {
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

export function testApplicationDirectoriesAreCreatedAndReadyToUse(): none {
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

export function testApplicationDirectoryFailsWhenTargetIsNotADirectory(): none {
  appId := "dev.doof.path-tests-file-conflict"
  directory := try! cacheDirectory(appId)

  try! remove(directory)
  try! writeText(directory, "not a directory")

  blocked := cacheDirectory(appId)
  assert(isFailure(blocked), "expected cacheDirectory to fail when the target path is a file")

  try! remove(directory)
}

export function testCurrentWorkingDirectoryAndSetterRoundTrip(): none {
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
