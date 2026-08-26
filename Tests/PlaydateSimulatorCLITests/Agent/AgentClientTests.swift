import Darwin
import Foundation
import SocketTestSupport
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent client transport")
struct AgentClientTests {
    @Test("Sends a command through the C transport", .timeLimit(.minutes(1)))
    func sendsCommand() async throws {
        let (response, request) = try await withAgentTestServer(response: "ok accepted") {
            socketPath in
            let client = AgentClient(socketPathProvider: { _ in socketPath })
            return try await client.send(.status, processIdentifier: getpid())
        }

        #expect(response == "ok accepted")
        #expect(request == "status")
    }

    @Test("Rejects a socket owned by another process", .timeLimit(.minutes(1)))
    func rejectsWrongPeer() async throws {
        do {
            _ = try await withAgentTestServer(response: "ok") { socketPath in
                let client = AgentClient(socketPathProvider: { _ in socketPath })
                return try await client.send(.status, processIdentifier: getpid() + 1)
            }
            Issue.record("Expected the peer process check to fail")
        } catch CLIError.agentUnavailable {
            // Expected: LOCAL_PEERPID did not match the selected Simulator process.
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Classifies a response deadline as a connection failure", .timeLimit(.minutes(1)))
    func classifiesTimeout() async throws {
        do {
            _ = try await withAgentTestServer(
                response: "ok delayed",
                responseDelayMilliseconds: 100
            ) { socketPath in
                let client = AgentClient(
                    socketPathProvider: { _ in socketPath },
                    connectTimeoutMilliseconds: 100,
                    responseTimeoutMilliseconds: 25
                )
                return try await client.send(.status, processIdentifier: getpid())
            }
            Issue.record("Expected the response deadline to expire")
        } catch CLIError.agentConnectionFailed {
            // Expected: the connection succeeded but no response arrived in time.
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Distinguishes an absent agent from other transport failures")
    func classifiesMissingAgent() async {
        let socketPath = "/tmp/playdate-simctl-missing-\(UUID().uuidString).sock"
        let client = AgentClient(
            socketPathProvider: { _ in socketPath },
            connectTimeoutMilliseconds: 100,
            responseTimeoutMilliseconds: 100
        )

        do {
            _ = try await client.send(.status, processIdentifier: getpid())
            Issue.record("Expected the missing socket to be reported")
        } catch CLIError.agentNotRunning {
            // Expected: no process owns the selected socket path.
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Treats a refused stale socket as a missing agent")
    func classifiesStaleSocket() async throws {
        let socketPath = "/tmp/playdate-simctl-stale-\(UUID().uuidString).sock"
        var errorMessage = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
        let created = socketPath.withCString { socketPathPointer in
            unsafe pdsim_test_create_stale_socket(
                socketPathPointer,
                &errorMessage,
                errorMessage.count
            )
        }
        try #require(
            created,
            "Could not create stale socket: \(decodeCString(errorMessage))"
        )
        defer { unlink(socketPath) }

        let client = AgentClient(
            socketPathProvider: { _ in socketPath },
            connectTimeoutMilliseconds: 100,
            responseTimeoutMilliseconds: 100
        )
        do {
            _ = try await client.send(.status, processIdentifier: getpid())
            Issue.record("Expected the stale socket to be recoverable")
        } catch CLIError.agentNotRunning {
            // Expected: a dead server left a socket path behind.
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Rejects carriage returns in response framing", .timeLimit(.minutes(1)))
    func rejectsCarriageReturnResponse() async throws {
        do {
            _ = try await withAgentTestServer(response: "ok\rspoofed") { socketPath in
                let client = AgentClient(socketPathProvider: { _ in socketPath })
                return try await client.send(.status, processIdentifier: getpid())
            }
            Issue.record("Expected malformed response framing to fail")
        } catch CLIError.agentUnavailable(let message) {
            #expect(message == "invalid agent response framing")
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    private func decodeCString(_ characters: [CChar]) -> String {
        String(
            decoding: characters.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) },
            as: UTF8.self
        )
    }

}
