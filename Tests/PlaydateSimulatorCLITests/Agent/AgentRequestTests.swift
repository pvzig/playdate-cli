import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Agent protocol")
struct AgentRequestTests {
    @Test(
        "Serializes commands",
        arguments: [
            (AgentRequest.status, "status"),
            (AgentRequest.button(buttonMask: 32, isDown: true), "button 32 1"),
            (AgentRequest.button(buttonMask: 32, isDown: false), "button 32 0"),
            (AgentRequest.press(buttonMask: 16, durationMilliseconds: 125), "press 16 125"),
            (AgentRequest.crank(90), "crank 90.0"),
            (AgentRequest.crankDock(isDocked: true), "crank-docked 1"),
            (
                AgentRequest.accelerometer(x: 0.25, y: -0.5, z: 1),
                "accelerometer 0.25 -0.5 1.0"
            ),
            (AgentRequest.press(buttonMask: 64, durationMilliseconds: 100), "press 64 100"),
            (AgentRequest.lock, "lock"),
            (AgentRequest.pause(isPaused: true), "pause 1"),
            (AgentRequest.pause(isPaused: false), "pause 0"),
            (AgentRequest.restart, "restart"),
            (AgentRequest.volume(.up(percent: 10)), "volume-adjust 10"),
            (AgentRequest.volume(.down(percent: 25)), "volume-adjust -25"),
            (AgentRequest.volume(.set(percent: 50)), "volume-set 50"),
            (AgentRequest.screenshot(path: "/tmp/screen.png"), "screenshot /tmp/screen.png"),
            (AgentRequest.toolbar(.memory), "toolbar lua-memory"),
            (AgentRequest.load(path: "/tmp/Example.pdx"), "load /tmp/Example.pdx"),
            (
                AgentRequest.setActivePDX(path: "/tmp/Example.pdx"),
                "set-active-pdx /tmp/Example.pdx"
            ),
            (
                AgentRequest.record(.start(path: "/tmp/Example.gif")),
                "record-start /tmp/Example.gif"
            ),
            (AgentRequest.record(.stop), "record-stop"),
        ]
    )
    func serializes(request: AgentRequest, expectedLine: String) {
        #expect(request.line == expectedLine)
    }

    @Test("Maps a CLI button command")
    func mapsCLICommand() throws {
        let request = try AgentRequest(command: .button(.down, .up))
        #expect(request == .button(buttonMask: 8, isDown: false))
    }

    @Test(
        "Maps recording commands",
        arguments: [
            (
                SimulatorCommand.record(.start(path: "/tmp/Example.gif")),
                AgentRequest.record(.start(path: "/tmp/Example.gif"))
            ),
            (SimulatorCommand.record(.stop), AgentRequest.record(.stop)),
        ]
    )
    func mapsRecordingCommand(command: SimulatorCommand, expectedRequest: AgentRequest) throws {
        #expect(try AgentRequest(command: command) == expectedRequest)
    }

    @Test("Does not treat project run as an agent request")
    func rejectsProjectRun() {
        #expect(throws: CLIError.invalidArgument("command is not an agent request")) {
            try AgentRequest(
                command: .run(
                    ProjectRun(
                        projectDirectoryURL: URL(filePath: "/tmp/project"),
                        productURL: URL(filePath: "/tmp/project/Game.pdx"),
                        buildTask: "build"
                    )
                )
            )
        }
    }
}
