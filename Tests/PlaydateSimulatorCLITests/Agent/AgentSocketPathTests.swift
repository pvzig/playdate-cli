import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent socket path")
struct AgentSocketPathTests {
    @Test("Scopes the socket by user and Simulator process")
    func scopesSocketPath() {
        #expect(
            AgentSocketPath.path(processIdentifier: 456, userIdentifier: 123)
                == "/tmp/playdate-simctl-123/456.sock"
        )
    }
}
