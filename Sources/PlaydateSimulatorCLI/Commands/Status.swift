import ArgumentParser

struct Status: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Show agent process and capability status."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  var simulatorCommand: SimulatorCommand { .status }
}
