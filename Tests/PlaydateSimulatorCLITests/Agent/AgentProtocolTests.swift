import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent protocol framing")
struct AgentProtocolTests {
    @Test("Accepts the maximum request size")
    func acceptsMaximumSize() throws {
        try AgentProtocol.validate(
            line: String(repeating: "a", count: AgentProtocol.maximumLineByteCount)
        )
    }

    @Test("Rejects oversized and multiline requests")
    func rejectsInvalidFraming() {
        #expect(
            throws: CLIError.invalidArgument(
                "agent command exceeds the \(AgentProtocol.maximumLineByteCount)-byte protocol limit"
            )
        ) {
            try AgentProtocol.validate(
                line: String(repeating: "a", count: AgentProtocol.maximumLineByteCount + 1)
            )
        }
        #expect(throws: CLIError.invalidArgument("agent commands cannot contain line breaks")) {
            try AgentProtocol.validate(line: "load /tmp/one.pdx\nload /tmp/two.pdx")
        }
        #expect(throws: CLIError.invalidArgument("agent commands cannot contain line breaks")) {
            try AgentProtocol.validate(line: "load /tmp/game.pdx\r")
        }
    }
}
