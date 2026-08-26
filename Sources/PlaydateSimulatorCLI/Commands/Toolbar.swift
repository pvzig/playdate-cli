import ArgumentParser

struct Toolbar: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    abstract: "Invoke an exact Simulator toolbar action."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  @Argument(help: "The toolbar action to invoke.")
  var action: ToolbarAction

  var simulatorCommand: SimulatorCommand {
    .toolbar(action)
  }
}
