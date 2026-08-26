import Foundation

struct AgentLocator: Sendable {
  func resolve(overridePath: String?) throws -> URL {
    let agentURL: URL
    if let overridePath {
      agentURL = URL(filePath: overridePath).standardizedFileURL
    } else if let executableURL = Bundle.main.executableURL {
      agentURL = Self.agentURL(adjacentTo: executableURL)
    } else {
      throw CLIError.agentUnavailable("could not locate the playdate-simctl executable")
    }

    guard FileManager.default.isReadableFile(atPath: agentURL.path) else {
      throw CLIError.agentUnavailable(
        "the injected agent was not found at \(agentURL.path); run `mise run build`"
      )
    }
    return agentURL
  }

  static func agentURL(adjacentTo executableURL: URL) -> URL {
    executableURL
      .resolvingSymlinksInPath()
      .deletingLastPathComponent()
      .appending(path: "libPlaydateSimulatorAgent.dylib")
  }
}
