import Testing

@testable import PlaydateSimulatorCLI

@Suite("Subprocess runner")
struct SubprocessRunnerTests {
  @Test("Truncates output without failing a successful subprocess")
  func truncatesOutput() async throws {
    let result = try await SubprocessRunner().run(
      executable: "/usr/bin/printf",
      arguments: ["1234567890"],
      outputLimit: 5
    )

    #expect(result.succeeded)
    #expect(result.output == "12345")
    #expect(result.outputWasTruncated)
  }

  @Test("Preserves cancellation")
  func preservesCancellation() async {
    let runner = SubprocessRunner { _, _, _, _ in
      throw CancellationError()
    }

    do {
      _ = try await runner.run(executable: "/usr/bin/example", arguments: [])
      Issue.record("Expected cancellation to be preserved.")
    } catch is CancellationError {
      // Expected: cancellation must not be converted into a CLI failure.
    } catch {
      Issue.record("Unexpected error: \(error)")
    }
  }
}
