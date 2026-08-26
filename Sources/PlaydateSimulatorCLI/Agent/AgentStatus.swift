import SocketSupport

enum AgentStatus {
  static func validate(response: String, expectedProcessIdentifier: Int32) throws {
    let fields = response.split(whereSeparator: \.isWhitespace)
    guard fields.first == "ok" else {
      throw CLIError.agentUnavailable("invalid agent status response")
    }

    let values = try Self.parseFields(fields.dropFirst())

    guard
      Int(values["protocol"] ?? "") == Int(PLAYDATE_SIMULATOR_AGENT_PROTOCOL_VERSION)
    else {
      throw CLIError.agentUnavailable(
        "the running Simulator has an incompatible control agent; quit and restart "
          + "Playdate Simulator before retrying"
      )
    }

    guard Int32(values["pid"] ?? "") == expectedProcessIdentifier else {
      throw CLIError.agentUnavailable("the injected agent reported the wrong process")
    }

    let requiredCapabilities = [
      "buttons", "crank", "dock", "accelerometer", "lock", "volume", "ui", "screenshot",
      "load",
      "record",
    ]
    let missingCapabilities = requiredCapabilities.filter { values[$0] != "1" }
    guard missingCapabilities.isEmpty else {
      throw CLIError.agentNotReady(
        "the Simulator agent is not ready; missing capabilities: "
          + missingCapabilities.joined(separator: ", ")
      )
    }
  }

  private static func parseFields(
    _ fields: ArraySlice<Substring>
  ) throws -> [String: String] {
    var values: [String: String] = [:]
    for field in fields {
      let components = field.split(separator: "=", maxSplits: 1)
      guard components.count == 2 else {
        throw CLIError.agentUnavailable("invalid agent status field")
      }

      let name = String(components[0])
      guard values[name] == nil else {
        throw CLIError.agentUnavailable("duplicate agent status field: \(name)")
      }
      values[name] = String(components[1])
    }
    return values
  }
}
