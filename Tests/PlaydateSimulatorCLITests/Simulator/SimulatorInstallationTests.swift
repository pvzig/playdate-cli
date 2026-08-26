import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Simulator installation")
struct SimulatorInstallationTests {
    @Test("Uses the SDK environment path")
    func usesSDKEnvironmentPath() {
        let installation = SimulatorInstallation(
            overridePath: nil,
            environment: ["PLAYDATE_SDK_PATH": "/opt/PlaydateSDK"]
        )

        #expect(
            installation.appURL.path
                == "/opt/PlaydateSDK/bin/Playdate Simulator.app"
        )
    }

    @Test("Explicit app path takes precedence over the SDK environment")
    func explicitPathTakesPrecedence() {
        let installation = SimulatorInstallation(
            overridePath: "/Applications/Playdate Simulator.app",
            environment: ["PLAYDATE_SDK_PATH": "/opt/PlaydateSDK"]
        )

        #expect(installation.appURL.path == "/Applications/Playdate Simulator.app")
    }
}
