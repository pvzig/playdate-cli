import ArgumentParser

struct Inject: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Inject the control agent and show its status."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  var simulatorCommand: SimulatorCommand { .inject }
}
