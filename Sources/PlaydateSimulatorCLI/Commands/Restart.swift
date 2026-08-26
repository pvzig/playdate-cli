import ArgumentParser

struct Restart: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Restart the loaded game."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  var simulatorCommand: SimulatorCommand { .restart }
}
