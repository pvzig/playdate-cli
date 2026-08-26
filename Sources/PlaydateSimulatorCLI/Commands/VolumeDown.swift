import ArgumentParser

struct VolumeDown: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        commandName: "down",
        abstract: "Decrease the Simulator volume."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Option(help: "The percentage-point adjustment.")
    var step = 10

    var simulatorCommand: SimulatorCommand {
        get throws {
            guard (1...100).contains(step) else {
                throw ValidationError("--step must be between 1 and 100")
            }
            return .volume(.down(percent: step))
        }
    }
}
