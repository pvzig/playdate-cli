import ArgumentParser

struct StartRecording: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        commandName: "start",
        abstract: "Start a GIF recording."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(
        help: "The new GIF output path.",
        completion: .file(extensions: ["gif"])
    )
    var outputPath: String

    var simulatorCommand: SimulatorCommand {
        get throws {
            let outputURL = PathValidation.resolve(outputPath)
            try PathValidation.requireExtension(
                "gif", for: outputURL, description: "recording output")
            try PathValidation.requireAvailableOutput(outputURL, description: "recording output")
            try AgentProtocol.validate(line: "record-start \(outputURL.path)")
            return .record(.start(path: outputURL.path))
        }
    }
}
