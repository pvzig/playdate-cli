import ArgumentParser

struct SetButton: SimulatorControlCommand {
  static let configuration = CommandConfiguration(
    commandName: "button",
    abstract: "Set a Playdate button's held state."
  )

  @OptionGroup var simulatorOptions: SimulatorOptions

  @Argument(help: "The Playdate button to update.")
  var button: Button

  @Argument(help: "The new button state.")
  var state: ButtonState

  var simulatorCommand: SimulatorCommand {
    .button(button, state)
  }
}
