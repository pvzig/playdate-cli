import Foundation

enum CLIError: Error, LocalizedError, Equatable {
  case agentNotRunning(String)
  case agentNotReady(String)
  case agentConnectionFailed(String)
  case agentUnavailable(String)
  case invalidArgument(String)
  case simulatorUnavailable(String)
  case subprocessFailed(String)

  var errorDescription: String? {
    switch self {
    case .agentNotRunning(let message),
      .agentNotReady(let message),
      .agentConnectionFailed(let message),
      .agentUnavailable(let message),
      .invalidArgument(let message),
      .simulatorUnavailable(let message),
      .subprocessFailed(let message):
      message
    }
  }
}
