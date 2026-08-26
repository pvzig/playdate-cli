enum VolumeAction: Equatable, Sendable {
  case up(percent: Int)
  case down(percent: Int)
  case set(percent: Int)

  var protocolLine: String {
    switch self {
    case .up(let percent):
      "volume-adjust \(percent)"
    case .down(let percent):
      "volume-adjust -\(percent)"
    case .set(let percent):
      "volume-set \(percent)"
    }
  }
}
