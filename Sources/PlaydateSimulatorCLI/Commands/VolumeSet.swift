import ArgumentParser

struct VolumeSet: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        commandName: "set",
        abstract: "Set the Simulator volume."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(help: "The volume percentage from 0 through 100.")
    var percent: Int

    var simulatorCommand: SimulatorCommand {
        get throws {
            guard (0...100).contains(percent) else {
                throw ValidationError("volume must be between 0 and 100")
            }
            return .volume(.set(percent: percent))
        }
    }
}
