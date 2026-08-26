import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Path validation")
struct PathValidationTests {
    @Test("Accepts an existing directory")
    func acceptsDirectory() throws {
        let directoryURL = FileManager.default.temporaryDirectory
            .appending(path: "playdate-simctl-\(UUID().uuidString)", directoryHint: .isDirectory)
        try FileManager.default.createDirectory(
            at: directoryURL, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: directoryURL) }

        #expect(throws: Never.self) {
            try PathValidation.requireDirectory(at: directoryURL, message: "expected a directory")
        }
    }

    @Test("Rejects a regular file")
    func rejectsRegularFile() throws {
        let fileURL = FileManager.default.temporaryDirectory
            .appending(path: "playdate-simctl-\(UUID().uuidString)")
        try Data().write(to: fileURL)
        defer { try? FileManager.default.removeItem(at: fileURL) }

        #expect(throws: CLIError.invalidArgument("expected a directory")) {
            try PathValidation.requireDirectory(at: fileURL, message: "expected a directory")
        }
    }
}
