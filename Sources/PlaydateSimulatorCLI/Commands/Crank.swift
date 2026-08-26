import ArgumentParser

struct Crank: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Set the crank position or docking state."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  @Argument(help: "A position from 0 through 360, or 'dock'/'undock'.")
  var position: String

  var simulatorCommand: SimulatorCommand {
    get throws {
      switch position {
      case "dock":
        return .crankDock(.docked)
      case "undock":
        return .crankDock(.undocked)
      default:
        break
      }

      guard let degrees = Float(position), degrees.isFinite else {
        throw ValidationError("crank position must be a finite number, 'dock', or 'undock'")
      }
      guard (0...360).contains(degrees) else {
        throw ValidationError("crank position must be between 0 and 360")
      }
      return .crank(degrees)
    }
  }
}
