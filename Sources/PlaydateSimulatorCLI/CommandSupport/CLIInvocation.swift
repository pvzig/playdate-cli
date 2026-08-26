struct CLIInvocation: Equatable, Sendable {
    let processIdentifier: Int32?
    let agentPath: String?
    let simulatorAppPath: String?
    let command: SimulatorCommand
}
