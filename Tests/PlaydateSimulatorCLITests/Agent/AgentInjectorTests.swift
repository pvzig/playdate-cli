import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent injector")
struct AgentInjectorTests {
    @Test("Accepts a non-null dlopen result")
    func acceptsSuccessfulLoad() async throws {
        let injector = AgentInjector(
            subprocessRunner: SubprocessRunner { executable, arguments, _, _ in
                #expect(executable == "/usr/bin/lldb")
                #expect(arguments.contains { $0.contains("dlerror()") })
                return SubprocessResult(
                    succeeded: true,
                    output:
                        "(const char *) $1 = 0x0000000100000000 \"playdate-simctl:dlopen-succeeded\""
                )
            }
        )

        try await injector.inject(
            agentURL: URL(filePath: "/tmp/agent.dylib"),
            processIdentifier: 123
        )
    }

    @Test("Ignores an echoed success marker when dlopen fails")
    func reportsLoadFailure() async {
        let injector = AgentInjector(
            subprocessRunner: SubprocessRunner { _, _, _, _ in
                SubprocessResult(
                    succeeded: true,
                    output: """
                        (lldb) expression -- (const char *)($pdsimAgentHandle == 0 ? (const char *)dlerror() : "playdate-simctl:dlopen-succeeded")
                        (const char *) $1 = 0x0000000100000000 "dlopen failed: image not found"
                        """
                )
            }
        )

        do {
            try await injector.inject(
                agentURL: URL(filePath: "/tmp/missing-agent.dylib"),
                processIdentifier: 123
            )
            Issue.record("Expected the failed dlopen result to be rejected.")
        } catch CLIError.subprocessFailed(let message) {
            #expect(message.contains("image not found"))
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }

    @Test("Escapes paths for an LLDB string literal")
    func escapesPath() throws {
        #expect(
            try AgentInjector.escapeForLLDBString("/tmp/a\\b\"c.dylib")
                == "/tmp/a\\\\b\\\"c.dylib"
        )
    }

    @Test("Rejects line breaks before constructing an LLDB expression", arguments: ["\r", "\n"])
    func rejectsLineBreak(lineBreak: String) async {
        let injector = AgentInjector(
            subprocessRunner: SubprocessRunner { _, _, _, _ in
                SubprocessResult(succeeded: true, output: "")
            }
        )
        let agentURL = URL(filePath: "/tmp/agent\(lineBreak)script.dylib")

        do {
            try await injector.inject(agentURL: agentURL, processIdentifier: 123)
            Issue.record("Expected the agent path to be rejected.")
        } catch CLIError.invalidArgument(let message) {
            #expect(message == "agent path must not contain carriage returns or newlines")
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }
}
