import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent status")
struct AgentStatusTests {
    @Test("Accepts the expected process and capabilities")
    func acceptsExpectedStatus() throws {
        try AgentStatus.validate(
            response:
                "ok protocol=1 pid=42 buttons=1 crank=1 dock=1 accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1",
            expectedProcessIdentifier: 42
        )
    }

    @Test("Rejects a missing capability")
    func rejectsMissingCapability() {
        do {
            try AgentStatus.validate(
                response:
                    "ok protocol=1 pid=42 buttons=1 crank=0 dock=1 accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1",
                expectedProcessIdentifier: 42
            )
            Issue.record("Expected an incomplete agent to be reported as not ready")
        } catch CLIError.agentNotReady(let message) {
            #expect(message.contains("crank"))
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Rejects duplicate fields")
    func rejectsDuplicateFields() {
        #expect(throws: CLIError.self) {
            try AgentStatus.validate(
                response:
                    "ok protocol=1 pid=42 pid=42 buttons=1 crank=1 dock=1 accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1",
                expectedProcessIdentifier: 42
            )
        }
    }

    @Test(
        "Rejects a missing or incompatible protocol version",
        arguments: [
            "ok pid=42 buttons=1 crank=1 dock=1 accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1",
            "ok protocol=2 pid=42 buttons=1 crank=1 dock=1 accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1",
        ])
    func rejectsIncompatibleProtocol(response: String) {
        do {
            try AgentStatus.validate(response: response, expectedProcessIdentifier: 42)
            Issue.record("Expected the incompatible protocol to be rejected")
        } catch CLIError.agentUnavailable(let message) {
            #expect(message.contains("quit and restart Playdate Simulator"))
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }
}
