import ArgumentParser

struct Load: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        abstract: "Load an existing PDX into Simulator."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(
        help: "The PDX directory bundle to load.",
        completion: .file(extensions: ["pdx"])
    )
    var productPath: String

    var simulatorCommand: SimulatorCommand {
        get throws {
            let productURL = PathValidation.resolve(productPath, directoryHint: .isDirectory)
            try PathValidation.requireExtension("pdx", for: productURL, description: "load product")
            try PathValidation.requireDirectory(
                at: productURL,
                message: "PDX directory bundle was not found at \(productURL.path)"
            )
            try AgentProtocol.validate(line: "load \(productURL.path)")
            return .load(path: productURL.path)
        }
    }
}
