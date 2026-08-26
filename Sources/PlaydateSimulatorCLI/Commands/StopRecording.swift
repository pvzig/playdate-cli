import ArgumentParser

struct StopRecording: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        commandName: "stop",
        abstract: "Stop and save the active GIF recording."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    var simulatorCommand: SimulatorCommand { .record(.stop) }
}
