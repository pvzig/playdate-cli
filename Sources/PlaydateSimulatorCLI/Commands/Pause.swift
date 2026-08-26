import ArgumentParser

struct Pause: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Pause Simulator."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  var simulatorCommand: SimulatorCommand { .pause }
}
