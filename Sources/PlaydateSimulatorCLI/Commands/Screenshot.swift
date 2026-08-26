import ArgumentParser

struct Screenshot: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        abstract: "Capture the Simulator framebuffer as a PNG."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(
        help: "The new PNG output path.",
        completion: .file(extensions: ["png"])
    )
    var outputPath: String

    var simulatorCommand: SimulatorCommand {
        get throws {
            let outputURL = PathValidation.resolve(outputPath)
            try PathValidation.requireExtension(
                "png", for: outputURL, description: "screenshot output")
            try PathValidation.requireAvailableOutput(outputURL, description: "screenshot output")
            try AgentProtocol.validate(line: "screenshot \(outputURL.path)")
            return .screenshot(path: outputURL.path)
        }
    }
}
