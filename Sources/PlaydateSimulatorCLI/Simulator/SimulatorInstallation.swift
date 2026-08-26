import Foundation

struct SimulatorInstallation: Sendable {
    let appURL: URL

    init(
        overridePath: String?,
        environment: [String: String] = ProcessInfo.processInfo.environment
    ) {
        if let overridePath {
            appURL = URL(filePath: overridePath).standardizedFileURL
        } else if let sdkPath = environment["PLAYDATE_SDK_PATH"], !sdkPath.isEmpty {
            appURL =
                URL(filePath: sdkPath)
                .appending(path: "bin")
                .appending(path: "Playdate Simulator.app")
                .standardizedFileURL
        } else {
            appURL = URL.homeDirectory
                .appending(path: "Developer")
                .appending(path: "PlaydateSDK")
                .appending(path: "bin")
                .appending(path: "Playdate Simulator.app")
        }
    }

    var executableURL: URL {
        appURL
            .appending(path: "Contents")
            .appending(path: "MacOS")
            .appending(path: "Playdate Simulator")
    }
}
