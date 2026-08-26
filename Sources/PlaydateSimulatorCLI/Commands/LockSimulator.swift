import ArgumentParser

struct LockSimulator: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        commandName: "lock",
        abstract: "Toggle the Simulator lock state."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    var simulatorCommand: SimulatorCommand { .lock }
}
