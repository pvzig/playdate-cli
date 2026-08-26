enum RecordingAction: Equatable, Sendable {
  case start(path: String)
  case stop

  var protocolLine: String {
    switch self {
    case .start(let path):
      "record-start \(path)"
    case .stop:
      "record-stop"
    }
  }
}
