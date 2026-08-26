import Darwin
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Simulator process locator")
struct SimulatorProcessLocatorTests {
    @Test("Scopes automatic process discovery to the current user")
    func scopesProcessDiscovery() async throws {
        let locator = SimulatorProcessLocator(
            subprocessRunner: SubprocessRunner { executable, arguments, _, _ in
                #expect(executable == "/usr/bin/pgrep")
                #expect(arguments == ["-U", String(getuid()), "-x", "Playdate Simulator"])
                return SubprocessResult(succeeded: true, output: "42\n")
            }
        )

        #expect(try await locator.resolve(explicitProcessIdentifier: nil) == 42)
    }

    @Test("Parses, filters, and sorts pgrep output")
    func parsesProcessIdentifiers() {
        let processIdentifiers = SimulatorProcessLocator.parseProcessIdentifiers(
            "42\nnot-a-pid\n7\n-1\n"
        )
        #expect(processIdentifiers == [7, 42])
    }

    @Test("Verifies the kernel-reported executable path")
    func verifiesExecutablePath() async throws {
        let installation = SimulatorInstallation(
            overridePath: "/Applications/Selected/Playdate Simulator.app"
        )
        let locator = SimulatorProcessLocator(
            executablePathProvider: { _ in installation.executableURL.path }
        )

        try await locator.verify(processIdentifier: 42, installation: installation)
    }

    @Test("Rejects a process whose executable does not match the selected installation")
    func rejectsDifferentExecutable() async {
        let installation = SimulatorInstallation(
            overridePath: "/Applications/Selected/Playdate Simulator.app"
        )
        let locator = SimulatorProcessLocator(
            executablePathProvider: { _ in
                "/Applications/Other/Playdate Simulator.app/Contents/MacOS/Playdate Simulator"
            }
        )

        do {
            try await locator.verify(processIdentifier: 42, installation: installation)
            Issue.record("Expected the executable mismatch to be rejected")
        } catch CLIError.simulatorUnavailable(let message) {
            #expect(message.contains("is not the selected Playdate Simulator"))
        } catch {
            Issue.record("Unexpected error: \(error)")
        }
    }
}
