import ArgumentParser
import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Command validation")
struct CommandValidationTests {
    @Test("Maps shared Simulator options")
    func mapsSimulatorOptions() throws {
        let status = try Status.parse([
            "--pid", "123",
            "--agent", "/tmp/agent.dylib",
            "--simulator-app", "/Applications/Playdate Simulator.app",
        ])

        #expect(
            try status.simulatorOptions.invocation(for: .status)
                == CLIInvocation(
                    processIdentifier: 123,
                    agentPath: "/tmp/agent.dylib",
                    simulatorAppPath: "/Applications/Playdate Simulator.app",
                    command: .status
                )
        )
    }

    @Test("Maps a project build and run")
    func mapsProjectRun() throws {
        let run = try Run.parse([
            ".build/Game.pdx",
            "--project-directory", "/tmp/example-project",
            "--build-task", "pdx",
        ])

        #expect(
            try run.simulatorCommand
                == .run(
                    ProjectRun(
                        projectDirectory: "/tmp/example-project",
                        productPath: "/tmp/example-project/.build/Game.pdx",
                        buildTask: "pdx"
                    )
                )
        )
    }

    @Test("Requires load products to be PDX directory bundles")
    func validatesLoadProduct() throws {
        let temporaryURL = FileManager.default.temporaryDirectory
            .appending(path: "playdate-simctl-\(UUID().uuidString).pdx")
        try Data().write(to: temporaryURL)
        defer { try? FileManager.default.removeItem(at: temporaryURL) }

        let load = try Load.parse([temporaryURL.path])
        #expect(throws: CLIError.self) {
            _ = try load.simulatorCommand
        }
    }

    @Test("Maps an existing PDX directory bundle")
    func mapsLoadProduct() throws {
        let temporaryURL = FileManager.default.temporaryDirectory
            .appending(
                path: "playdate-simctl-\(UUID().uuidString).pdx", directoryHint: .isDirectory)
        try FileManager.default.createDirectory(
            at: temporaryURL, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: temporaryURL) }

        #expect(
            try Load.parse([temporaryURL.path]).simulatorCommand == .load(path: temporaryURL.path))
    }

    @Test("Requires a PDX extension for project runs")
    func validatesRunProductExtension() throws {
        let run = try Run.parse([".build/Game"])
        #expect(throws: CLIError.self) {
            _ = try run.simulatorCommand
        }
    }

    @Test("Maps press targets and durations")
    func mapsPress() throws {
        #expect(
            try Press.parse(["a"]).simulatorCommand
                == .press(button: .a, durationMilliseconds: 100)
        )
        #expect(
            try Press.parse(["b", "--duration-ms", "250"]).simulatorCommand
                == .press(button: .b, durationMilliseconds: 250)
        )
        #expect(try Press.parse(["lock"]).simulatorCommand == .lock)
    }

    @Test("Rejects invalid press durations")
    func rejectsInvalidPressDuration() throws {
        let press = try Press.parse(["a", "--duration-ms", "10001"])
        #expect(throws: ValidationError.self) {
            _ = try press.simulatorCommand
        }
    }

    @Test("Maps crank positions and docking")
    func mapsCrank() throws {
        #expect(try Crank.parse(["90"]).simulatorCommand == .crank(90))
        #expect(try Crank.parse(["dock"]).simulatorCommand == .crankDock(.docked))
        #expect(try Crank.parse(["undock"]).simulatorCommand == .crankDock(.undocked))
    }

    @Test("Maps negative accelerometer components without an option terminator")
    func mapsNegativeAccelerometerComponents() throws {
        #expect(
            try Accelerometer.parse(["-0.25", "0.5", "-1"]).simulatorCommand
                == .accelerometer(x: -0.25, y: 0.5, z: -1)
        )
    }

    @Test("Rejects invalid crank positions", arguments: ["infinity", "-0.1", "360.1"])
    func rejectsInvalidCrank(position: String) throws {
        let crank = try Crank.parse(["--", position])
        #expect(throws: ValidationError.self) {
            _ = try crank.simulatorCommand
        }
    }

    @Test("Maps volume subcommands")
    func mapsVolume() throws {
        #expect(try VolumeUp.parse([]).simulatorCommand == .volume(.up(percent: 10)))
        #expect(
            try VolumeDown.parse(["--step", "25"]).simulatorCommand
                == .volume(.down(percent: 25))
        )
        #expect(try VolumeSet.parse(["0"]).simulatorCommand == .volume(.set(percent: 0)))
    }

    @Test("Rejects invalid volume percentages")
    func rejectsInvalidVolume() throws {
        let adjustment = try VolumeUp.parse(["--step", "0"])
        #expect(throws: ValidationError.self) {
            _ = try adjustment.simulatorCommand
        }

        let setting = try VolumeSet.parse(["101"])
        #expect(throws: ValidationError.self) {
            _ = try setting.simulatorCommand
        }
    }

    @Test("Maps documented toolbar actions")
    func mapsToolbarActions() throws {
        #expect(try Toolbar.parse(["lua-memory"]).simulatorCommand == .toolbar(.memory))
        #expect(try Toolbar.parse(["gif"]).simulatorCommand == .toolbar(.record))
        #expect(try Toolbar.parse(["controls"]).simulatorCommand == .toolbar(.controls))
    }

    @Test("Maps recording subcommands")
    func mapsRecordingCommands() throws {
        let outputURL = FileManager.default.temporaryDirectory
            .appending(path: "playdate-simctl-\(UUID().uuidString).gif")

        #expect(
            try StartRecording.parse([outputURL.path]).simulatorCommand
                == .record(.start(path: outputURL.path))
        )
        #expect(try StopRecording.parse([]).simulatorCommand == .record(.stop))
    }

    @Test(
        "Rejects paths that cannot be framed for the agent",
        arguments: [
            ("screenshot", "/tmp/one\ntwo.png"),
            ("record", "/tmp/one\ntwo.gif"),
            ("run", "/tmp/one\ntwo.pdx"),
        ]
    )
    func rejectsUnframeablePath(command: String, path: String) throws {
        #expect(throws: CLIError.self) {
            switch command {
            case "screenshot":
                _ = try Screenshot.parse([path]).simulatorCommand
            case "record":
                _ = try StartRecording.parse([path]).simulatorCommand
            case "run":
                _ = try Run.parse([path]).simulatorCommand
            default:
                Issue.record("Unknown fixture command: \(command)")
            }
        }
    }
}
