import Foundation
import SocketSupport
import SocketTestSupport
import Testing

@testable import PlaydateSimulatorCLI

func withAgentTestServer<Value: Sendable>(
  response: String,
  responseDelayMilliseconds: UInt32 = 0,
  operation: (String) async throws -> Value
) async throws -> (value: Value, request: String) {
  let socketPath = "/tmp/playdate-simctl-test-\(UUID().uuidString).sock"
  var startError = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
  let server = socketPath.withCString { socketPathPointer in
    response.withCString { responsePointer in
      unsafe pdsim_test_server_start(
        socketPathPointer,
        responsePointer,
        responseDelayMilliseconds,
        &startError,
        startError.count
      )
    }
  }
  let runningServer = try unsafe #require(
    server,
    "Could not start test server: \(decodeProtocolCString(startError))"
  )

  do {
    let value = try await operation(socketPath)
    return (value, try unsafe finishAgentTestServer(runningServer))
  } catch {
    _ = try? unsafe finishAgentTestServer(runningServer)
    throw error
  }
}

private func finishAgentTestServer(_ server: OpaquePointer) throws -> String {
  var request = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
  var errorMessage = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
  let succeeded = unsafe pdsim_test_server_finish(
    server,
    &request,
    request.count,
    &errorMessage,
    errorMessage.count
  )
  guard succeeded else {
    throw CLIError.agentConnectionFailed(
      "test server failed: \(decodeProtocolCString(errorMessage))"
    )
  }
  return decodeProtocolCString(request)
}

private func decodeProtocolCString(_ characters: [CChar]) -> String {
  let bytes = characters.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
  return String(decoding: bytes, as: UTF8.self)
}
