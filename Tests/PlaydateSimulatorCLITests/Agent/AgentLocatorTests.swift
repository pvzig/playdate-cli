import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent locator")
struct AgentLocatorTests {
    @Test("Resolves the agent beside a symlinked executable's target")
    func resolvesSymlinkedExecutable() throws {
        let temporaryDirectory = FileManager.default.temporaryDirectory
            .appending(path: "playdate-simctl-agent-locator-\(UUID().uuidString)")
        let installationDirectory = temporaryDirectory.appending(path: "installation")
        let linkDirectory = temporaryDirectory.appending(path: "bin")
        try FileManager.default.createDirectory(
            at: installationDirectory,
            withIntermediateDirectories: true
        )
        try FileManager.default.createDirectory(
            at: linkDirectory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: temporaryDirectory) }

        let executableURL = installationDirectory.appending(path: "playdate-simctl")
        let linkURL = linkDirectory.appending(path: "playdate-simctl")
        let agentURL = installationDirectory.appending(path: "libPlaydateSimulatorAgent.dylib")
        #expect(FileManager.default.createFile(atPath: executableURL.path, contents: Data()))
        #expect(FileManager.default.createFile(atPath: agentURL.path, contents: Data()))
        try FileManager.default.createSymbolicLink(at: linkURL, withDestinationURL: executableURL)

        #expect(AgentLocator.agentURL(adjacentTo: linkURL) == agentURL)
    }
}
