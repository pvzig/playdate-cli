import Foundation

struct ProjectRunner: Sendable {
    let subprocessRunner: SubprocessRunner

    init(subprocessRunner: SubprocessRunner = SubprocessRunner()) {
        self.subprocessRunner = subprocessRunner
    }

    func build(_ projectRun: ProjectRun) async throws {
        try PathValidation.requireDirectory(
            at: URL(filePath: projectRun.projectDirectory, directoryHint: .isDirectory),
            message: "project directory does not exist: \(projectRun.projectDirectory)"
        )

        let buildResult = try await subprocessRunner.run(
            executable: "/usr/bin/env",
            arguments: ["mise", "run", projectRun.buildTask],
            workingDirectory: projectRun.projectDirectory,
            outputLimit: 1024 * 1024
        )
        guard buildResult.succeeded else {
            throw CLIError.subprocessFailed(
                "mise task '\(projectRun.buildTask)' failed:\n\(buildResult.output)"
            )
        }

        try PathValidation.requireDirectory(
            at: URL(filePath: projectRun.productPath),
            message:
                "build completed but no PDX directory bundle was found at \(projectRun.productPath)"
        )
    }

    func launch(
        _ projectRun: ProjectRun,
        installation: SimulatorInstallation
    ) async throws {
        let launchResult = try await subprocessRunner.run(
            executable: "/usr/bin/open",
            arguments: ["-a", installation.appURL.path, "--args", projectRun.productPath]
        )
        guard launchResult.succeeded else {
            throw CLIError.subprocessFailed(
                "could not load the PDX in Playdate Simulator: \(launchResult.output)"
            )
        }
    }
}
