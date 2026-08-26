import ArgumentParser

struct SimulatorOptions: ParsableArguments {
    @Option(
        name: .customLong("pid"),
        help: "Select a running Simulator process by process identifier."
    )
    var processIdentifier: Int32?

    @Option(
        name: .customLong("agent"),
        help: "Override the injected agent path.",
        completion: .file(extensions: ["dylib"])
    )
    var agentPath: String?

    @Option(
        name: .customLong("simulator-app"),
        help: "Override the Playdate Simulator app path.",
        completion: .directory
    )
    var simulatorAppPath: String?

    func invocation(for command: SimulatorCommand) throws -> CLIInvocation {
        if let processIdentifier, processIdentifier <= 0 {
            throw ValidationError("--pid must be a positive process identifier")
        }

        return CLIInvocation(
            processIdentifier: processIdentifier,
            agentPath: agentPath,
            simulatorAppPath: simulatorAppPath,
            command: command
        )
    }
}
