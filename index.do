import function _homeDirectory(): Result<string, string> from "native_path.hpp" as doof_path::homeDirectory
import function _tempDirectory(): string from "native_path.hpp" as doof_path::tempDirectory
import function _currentWorkingDirectory(): Result<string, string> from "native_path.hpp" as doof_path::currentWorkingDirectory
export import function setCurrentWorkingDirectory(path: string): Result<void, string> from "native_path.hpp" as doof_path::setCurrentWorkingDirectory

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

export function currentWorkingDirectory(): Result<string, string> {
  return normalizePathResult(_currentWorkingDirectory())
}

export function join(parts: string[]): string {
  let absolute = false
  let segments: string[] = []

  for part of parts {
    if part.length == 0 {
      continue
    }

    if isAbsolute(part) {
      absolute = true
      segments = []
    }

    rawSegments := part.split("/")
    for rawSegment of rawSegments {
      if rawSegment.length == 0 || rawSegment == "." {
        continue
      }

      if rawSegment == ".." {
        if segments.length > 0 && segments[segments.length - 1] != ".." {
          segments = segments.slice(0, segments.length - 1)
        } else if !absolute {
          segments.push("..")
        }
        continue
      }

      segments.push(rawSegment)
    }
  }

  return renderPath(segments, absolute)
}

export function dirname(path: string): string {
  normalized := join([path])
  if normalized == "/" {
    return "/"
  }

  separator := lastSeparatorIndex(normalized)
  if separator < 0 {
    return "."
  }
  if separator == 0 {
    return "/"
  }
  return normalized.substring(0, separator)
}

export function basename(path: string): string {
  normalized := join([path])
  if normalized == "/" {
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
  return path.startsWith("/")
}

function renderPath(segments: string[], absolute: bool): string {
  if segments.length == 0 {
    return if absolute then "/" else "."
  }

  let output = segments[0]
  for index of 1..<segments.length {
    output += "/" + segments[index]
  }
  if absolute {
    return "/" + output
  }
  return output
}

function lastSeparatorIndex(path: string): int {
  let index = path.length - 1
  while index >= 0 {
    if path.charAt(index) == "/" {
      return index
    }
    index -= 1
  }
  return -1
}

function lastDotIndex(path: string): int {
  let index = path.length - 1
  while index >= 0 {
    if path.charAt(index) == "." {
      return index
    }
    index -= 1
  }
  return -1
}