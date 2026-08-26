import Darwin
import Foundation

struct SimulatorProcessLocator: Sendable {
    typealias ExecutablePathProvider = @Sendable (Int32) -> String?

    let subprocessRunner: SubprocessRunner
    let executablePathProvider: ExecutablePathProvider

    init(
        subprocessRunner: SubprocessRunner = SubprocessRunner(),
        executablePathProvider: @escaping ExecutablePathProvider = Self.executablePath
    ) {
        self.subprocessRunner = subprocessRunner
        self.executablePathProvider = executablePathProvider
    }

    func resolve(explicitProcessIdentifier: Int32?) async throws -> Int32 {
        guard
            let processIdentifier = try await resolveIfRunning(
                explicitProcessIdentifier: explicitProcessIdentifier
            )
        else {
            throw CLIError.simulatorUnavailable(
                "Playdate Simulator is not running; start it or pass --pid"
            )
        }
        return processIdentifier
    }

    func resolveIfRunning(explicitProcessIdentifier: Int32?) async throws -> Int32? {
        if let explicitProcessIdentifier {
            return explicitProcessIdentifier
        }

        let result = try await subprocessRunner.run(
            executable: "/usr/bin/pgrep",
            arguments: ["-U", String(getuid()), "-x", "Playdate Simulator"]
        )
        let processIdentifiers = Self.parseProcessIdentifiers(result.output)

        guard !processIdentifiers.isEmpty else {
            return nil
        }
        guard processIdentifiers.count == 1 else {
            let choices = processIdentifiers.map(String.init).joined(separator: ", ")
            throw CLIError.simulatorUnavailable(
                "multiple Playdate Simulator processes are running (\(choices)); pass --pid"
            )
        }
        return processIdentifiers[0]
    }

    /// Prevents `--pid` from turning the injector into a general-purpose
    /// arbitrary-process attachment mechanism.
    func verify(
        processIdentifier: Int32,
        installation: SimulatorInstallation
    ) async throws {
        guard let reportedPath = executablePathProvider(processIdentifier) else {
            throw CLIError.simulatorUnavailable(
                "could not inspect the executable for process \(processIdentifier)"
            )
        }

        let actualPath = URL(filePath: reportedPath)
            .resolvingSymlinksInPath()
            .standardizedFileURL.path
        let expectedPath = installation.executableURL
            .resolvingSymlinksInPath()
            .standardizedFileURL.path
        guard actualPath == expectedPath else {
            throw CLIError.simulatorUnavailable(
                "process \(processIdentifier) is not the selected Playdate Simulator"
            )
        }
    }

    static func parseProcessIdentifiers(_ output: String) -> [Int32] {
        output
            .split(whereSeparator: \.isWhitespace)
            .compactMap { Int32($0) }
            .filter { $0 > 0 }
            .sorted()
    }

    private static func executablePath(processIdentifier: Int32) -> String? {
        var path = [CChar](repeating: 0, count: Int(MAXPATHLEN) * 4)
        let length = unsafe proc_pidpath(
            processIdentifier,
            &path,
            UInt32(path.count)
        )
        guard length > 0, length < path.count else {
            return nil
        }

        return String(
            bytes: path.prefix(Int(length)).map { UInt8(bitPattern: $0) },
            encoding: .utf8
        )
    }
}
