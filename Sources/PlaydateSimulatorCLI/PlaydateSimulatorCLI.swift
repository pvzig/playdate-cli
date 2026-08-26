import ArgumentParser

@main
struct PlaydateSimulatorCLI: AsyncParsableCommand {
  static let configuration = CommandConfiguration(
    commandName: "playdate-simctl",
    abstract: "Control Playdate Simulator from the command line.",
    subcommands: [
      Run.self,
      Status.self,
      Load.self,
      Press.self,
      SetButton.self,
      LockSimulator.self,
      Pause.self,
      Resume.self,
      Restart.self,
      Crank.self,
      Accelerometer.self,
      Volume.self,
      Screenshot.self,
      Record.self,
      Toolbar.self,
      Inject.self,
    ]
  )
}
