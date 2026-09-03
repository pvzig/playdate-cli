import ArgumentParser
import Foundation

struct Run: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        abstract: "Build a project, launch Simulator, inject the agent, and load its PDX."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(
        help: "The PDX product path, relative to the project directory.",
        completion: .file(extensions: ["pdx"])
    )
    var productPath: String

    @Option(
        name: .customLong("project-directory"),
        help: "The project directory in which to run the build task.",
        completion: .directory
    )
    var projectDirectory = FileManager.default.currentDirectoryPath

    @Option(
        name: .customLong("build-task"),
        help: "The mise task that builds the PDX."
    )
    var buildTask = "build"

    var simulatorCommand: SimulatorCommand {
        get throws {
            guard buildTask.isEmpty == false else {
                throw ValidationError("--build-task cannot be empty")
            }

            let projectURL = PathValidation.resolve(
                projectDirectory,
                directoryHint: .isDirectory
            )
            let productURL = PathValidation.resolve(productPath, relativeTo: projectURL)
            try PathValidation.requireExtension("pdx", for: productURL, description: "run product")
            try AgentProtocol.validate(line: "load \(productURL.path)")

            return .run(
                ProjectRun(
                    projectDirectoryURL: projectURL,
                    productURL: productURL,
                    buildTask: buildTask
                )
            )
        }
    }
}
