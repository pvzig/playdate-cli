import SocketSupport

enum AgentProtocol {
  static let maximumLineByteCount = Int(PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES)
  static let bufferCapacity = Int(PDSIM_PROTOCOL_BUFFER_CAPACITY)

  static func validate(line: String) throws {
    guard !line.contains("\n"), !line.contains("\r") else {
      throw CLIError.invalidArgument("agent commands cannot contain line breaks")
    }
    guard line.utf8.count <= maximumLineByteCount else {
      throw CLIError.invalidArgument(
        "agent command exceeds the \(maximumLineByteCount)-byte protocol limit"
      )
    }
  }
}
