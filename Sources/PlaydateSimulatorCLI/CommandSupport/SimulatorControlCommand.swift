import ArgumentParser

protocol SimulatorControlCommand: AsyncParsableCommand {
    var simulatorOptions: SimulatorOptions { get }
    var simulatorCommand: SimulatorCommand { get throws }
}

extension SimulatorControlCommand {
    mutating func run() async throws {
        let invocation = try simulatorOptions.invocation(for: simulatorCommand)
        let response = try await SimulatorController().run(invocation)
        print(response)
    }
}
