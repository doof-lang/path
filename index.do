import isolated function _homeDirectory(): Result<string, string> from "native_path.hpp" as doof_path::homeDirectory
import isolated function _tempDirectory(): string from "native_path.hpp" as doof_path::tempDirectory
import isolated function _dataDirectory(appId: string | none = none): Result<string, string> from "native_path.hpp" as doof_path::dataDirectory
import isolated function _cacheDirectory(appId: string | none = none): Result<string, string> from "native_path.hpp" as doof_path::cacheDirectory
import isolated function _currentWorkingDirectory(): Result<string, string> from "native_path.hpp" as doof_path::currentWorkingDirectory
import isolated function _absolute(path: string): Result<string, string> from "native_path.hpp" as doof_path::absolute
import isolated function _resourcesDirectory(): Result<string, string> from "native_path.hpp" as doof_path::resourcesDirectory
export import function setCurrentWorkingDirectory(path: string): Result<none, string> from "native_path.hpp" as doof_path::setCurrentWorkingDirectory

function normalizePathResult(result: Result<string, string>): Result<string, string> {
  return case result {
    s: Success -> Success { value: join([s.value]) },
    f: Failure -> Failure { error: f.error }
  }
}

export function homeDirectory(): Result<string, string> {
  return normalizePathResult(_homeDirectory())
}

export function tempDirectory(): string {
  return join([_tempDirectory()])
}

export function dataDirectory(appId: string | none = none): Result<string, string> {
  return normalizePathResult(_dataDirectory(appId))
}

export function cacheDirectory(appId: string | none = none): Result<string, string> {
  return normalizePathResult(_cacheDirectory(appId))
}

export function currentWorkingDirectory(): Result<string, string> {
  return normalizePathResult(_currentWorkingDirectory())
}

export function absolute(path: string): Result<string, string> {
  return normalizePathResult(_absolute(join([path])))
}

export function resourcesDirectory(): Result<string, string> {
  return normalizePathResult(_resourcesDirectory())
}

export function resourcePath(path: string): Result<string, string> {
  try resources := resourcesDirectory()
  resolved := join([resources, path])

  if resolved == resources || resolved.startsWith(resources + "/") {
    return Success { value: resolved }
  }

  return Failure { error: "Resource path cannot escape the resources directory" }
}

export function join(parts: string[]): string {
  let prefix = ""
  let segments: string[] = []

  for part of parts {
    if part.length == 0 {
      continue
    }

    normalizedPart := part.replaceAll("\\", "/")
    partPrefix := rootPrefix(normalizedPart)
    if partPrefix != "" {
      prefix = partPrefix
      segments = []
    }

    let segmentSource = normalizedPart
    if partPrefix != "" {
      segmentSource = normalizedPart.slice(partPrefix.length)
      if segmentSource.startsWith("/") {
        segmentSource = segmentSource.slice(1)
      }
    }
    rawSegments := segmentSource.split("/")
    for rawSegment of rawSegments {
      if rawSegment.length == 0 || rawSegment == "." {
        continue
      }

      if rawSegment == ".." {
        if segments.length > 0 && segments[segments.length - 1] != ".." {
          segments = segments.slice(0, segments.length - 1)
        } else if prefix == "" {
          segments.push("..")
        }
        continue
      }

      segments.push(rawSegment)
    }
  }

  return renderPath(segments, prefix)
}

export function dirname(path: string): string {
  normalized := join([path])
  prefix := rootPrefix(normalized)
  if normalized == "/" || (prefix != "" && normalized == prefix + "/") {
    return normalized
  }

  separator := lastSeparatorIndex(normalized)
  if separator < 0 {
    return "."
  }
  if separator == 0 {
    return "/"
  }
  if prefix != "" && separator == prefix.length {
    return prefix + "/"
  }
  return normalized.substring(0, separator)
}

export function basename(path: string): string {
  normalized := join([path])
  prefix := rootPrefix(normalized)
  if normalized == "/" || (prefix != "" && normalized == prefix + "/") {
    return ""
  }

  separator := lastSeparatorIndex(normalized)
  if separator < 0 {
    return normalized
  }
  return normalized.slice(separator + 1)
}

export function stem(path: string): string {
  name := basename(path)
  if name == "" || name == "." || name == ".." {
    return name
  }

  dotIndex := lastDotIndex(name)
  if dotIndex <= 0 {
    return name
  }
  return name.substring(0, dotIndex)
}

export function extension(path: string): string {
  name := basename(path)
  if name == "" || name == "." || name == ".." {
    return ""
  }

  dotIndex := lastDotIndex(name)
  if dotIndex <= 0 {
    return ""
  }
  return name.slice(dotIndex)
}

export function isAbsolute(path: string): bool {
  return rootPrefix(path.replaceAll("\\", "/")) != ""
}

function rootPrefix(path: string): string {
  if path.startsWith("//") {
    components := path.split("/")
    if components.length >= 4 && components[2] != "" && components[3] != "" {
      return "//" + components[2] + "/" + components[3]
    }
  }
  if path.startsWith("/") {
    return "/"
  }
  if path.length >= 3 && isAsciiLetter(path.charAt(0)) && path.charAt(1) == ':' && path.charAt(2) == '/' {
    return path.substring(0, 2)
  }
  return ""
}

function isAsciiLetter(character: char): bool {
  return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
}

function renderPath(segments: string[], prefix: string): string {
  if segments.length == 0 {
    return if prefix == "" then "." else if prefix == "/" then "/" else prefix + "/"
  }

  let output = segments[0]
  for index of 1..<segments.length {
    output += "/" + segments[index]
  }
  if prefix == "/" {
    return "/" + output
  }
  if prefix != "" {
    return prefix + "/" + output
  }
  return output
}

function lastSeparatorIndex(path: string): int {
  let index = path.length - 1
  while index >= 0 {
    if path.charAt(index) == '/' {
      return index
    }
    index -= 1
  }
  return -1
}

function lastDotIndex(path: string): int {
  let index = path.length - 1
  while index >= 0 {
    if path.charAt(index) == '.' {
      return index
    }
    index -= 1
  }
  return -1
}
