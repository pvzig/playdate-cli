import Foundation

enum PathValidation {
  static var currentDirectoryURL: URL {
    URL(
      filePath: FileManager.default.currentDirectoryPath,
      directoryHint: .isDirectory
    )
  }

  static func resolve(
    _ path: String,
    relativeTo baseURL: URL? = nil,
    directoryHint: URL.DirectoryHint = .inferFromPath
  ) -> URL {
    URL(
      filePath: path,
      directoryHint: directoryHint,
      relativeTo: baseURL ?? currentDirectoryURL
    ).standardizedFileURL
  }

  static func requireExtension(
    _ pathExtension: String,
    for url: URL,
    description: String
  ) throws {
    guard url.pathExtension.lowercased() == pathExtension else {
      throw CLIError.invalidArgument(
        "\(description) must use the .\(pathExtension) extension"
      )
    }
  }

  static func requireAvailableOutput(_ url: URL, description: String) throws {
    guard (try? url.checkResourceIsReachable()) != true else {
      throw CLIError.invalidArgument("\(description) already exists: \(url.path)")
    }
  }

  static func requireDirectory(at url: URL, message: String) throws {
    let resourceValues = try? url.resourceValues(forKeys: [.isDirectoryKey])
    guard resourceValues?.isDirectory == true else {
      throw CLIError.invalidArgument(message)
    }
  }
}
