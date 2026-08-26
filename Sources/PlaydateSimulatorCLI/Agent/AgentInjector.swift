import Foundation

struct AgentInjector: Sendable {
    private static let successMarker = "playdate-simctl:dlopen-succeeded"

    let subprocessRunner: SubprocessRunner

    init(subprocessRunner: SubprocessRunner = SubprocessRunner()) {
        self.subprocessRunner = subprocessRunner
    }

    func inject(agentURL: URL, processIdentifier: Int32) async throws {
        let escapedAgentPath = try Self.escapeForLLDBString(agentURL.path)
        let loadExpression =
            "expression -- void *$pdsimAgentHandle = (void *)dlopen(\"\(escapedAgentPath)\", 0x2)"
        let resultExpression =
            "expression -- (const char *)($pdsimAgentHandle == 0 ? (const char *)dlerror() : \"\(Self.successMarker)\")"
        let result = try await subprocessRunner.run(
            executable: "/usr/bin/lldb",
            arguments: [
                "--no-lldbinit",
                "--batch",
                "--attach-pid",
                String(processIdentifier),
                "--one-line",
                loadExpression,
                "--one-line",
                resultExpression,
                "--one-line",
                "detach",
            ]
        )

        guard result.succeeded else {
            throw CLIError.subprocessFailed(
                "LLDB could not inject the Simulator agent: \(result.output)"
            )
        }
        guard Self.containsSuccessfulLoadResult(result.output) else {
            throw CLIError.subprocessFailed(
                "LLDB attached, but dlopen could not load the Simulator agent: \(result.output)"
            )
        }
    }

    private static func containsSuccessfulLoadResult(_ output: String) -> Bool {
        output.split(whereSeparator: \.isNewline).contains { rawLine in
            let line = rawLine.trimmingCharacters(in: .whitespaces)
            guard
                line.hasPrefix("(const char *) $"),
                let separator = line.range(of: " = ")
            else {
                return false
            }

            let value = line[separator.upperBound...]
            return value == successMarker
                || value == "\"\(successMarker)\""
                || value.hasSuffix(" \"\(successMarker)\"")
        }
    }

    static func escapeForLLDBString(_ value: String) throws -> String {
        guard value.contains("\r") == false, value.contains("\n") == false else {
            throw CLIError.invalidArgument(
                "agent path must not contain carriage returns or newlines"
            )
        }

        return
            value
            .replacing("\\", with: "\\\\")
            .replacing("\"", with: "\\\"")
    }
}
