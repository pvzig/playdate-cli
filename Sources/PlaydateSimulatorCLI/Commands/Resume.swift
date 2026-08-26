import ArgumentParser

struct Resume: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Resume Simulator."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  var simulatorCommand: SimulatorCommand { .resume }
}
