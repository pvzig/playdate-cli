import ArgumentParser
import SocketSupport

struct Press: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        abstract: "Press and release a Playdate button."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(help: "A Playdate button, or 'lock'.")
    var target: String

    @Option(
        name: .customLong("duration-ms"),
        help: "The press duration in milliseconds."
    )
    var durationMilliseconds: Int?

    var simulatorCommand: SimulatorCommand {
        get throws {
            if target == "lock" {
                guard durationMilliseconds == nil else {
                    throw ValidationError("press lock does not accept --duration-ms")
                }
                return .lock
            }

            guard let button = Button(rawValue: target) else {
                throw ValidationError("unknown button '\(target)'")
            }
            let durationMilliseconds = durationMilliseconds ?? 100
            let maximumDurationMilliseconds = Int(PDSIM_MAX_PRESS_MILLISECONDS)
            guard (1...maximumDurationMilliseconds).contains(durationMilliseconds) else {
                throw ValidationError(
                    "--duration-ms must be between 1 and \(maximumDurationMilliseconds)"
                )
            }
            return .press(button: button, durationMilliseconds: durationMilliseconds)
        }
    }
}
