import Darwin
import SocketSupport

enum AgentSocketPath {
  /// User and process identifiers always fit within the bounded protocol buffer.
  static func path(processIdentifier: Int32, userIdentifier: uid_t = getuid()) -> String {
    var path = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
    let byteCount = pdsim_socket_path(
      &path,
      path.count,
      userIdentifier,
      processIdentifier
    )
    precondition(byteCount >= 0 && byteCount < path.count)
    let bytes = path.prefix(Int(byteCount)).map { UInt8(bitPattern: $0) }
    return String(decoding: bytes, as: UTF8.self)
  }
}
