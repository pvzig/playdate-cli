enum AgentRequest: Equatable, Sendable {
    case status
    case press(buttonMask: Int32, durationMilliseconds: Int)
    case button(buttonMask: Int32, isDown: Bool)
    case crank(Float)
    case crankDock(isDocked: Bool)
    case accelerometer(x: Float, y: Float, z: Float)
    case lock
    case pause(isPaused: Bool)
    case restart
    case volume(VolumeAction)
    case screenshot(path: String)
    case toolbar(ToolbarAction)
    case load(path: String)
    case setActivePDX(path: String)
    case record(RecordingAction)

    init(command: SimulatorCommand) throws {
        switch command {
        case .status, .inject:
            self = .status
        case .press(let button, let durationMilliseconds):
            self = .press(
                buttonMask: button.mask,
                durationMilliseconds: durationMilliseconds
            )
        case .button(let button, let state):
            self = .button(buttonMask: button.mask, isDown: state == .down)
        case .crank(let degrees):
            self = .crank(degrees)
        case .crankDock(let state):
            self = .crankDock(isDocked: state == .docked)
        case .accelerometer(let x, let y, let z):
            self = .accelerometer(x: x, y: y, z: z)
        case .lock:
            self = .lock
        case .pause:
            self = .pause(isPaused: true)
        case .resume:
            self = .pause(isPaused: false)
        case .restart:
            self = .restart
        case .volume(let action):
            self = .volume(action)
        case .screenshot(let path):
            self = .screenshot(path: path)
        case .toolbar(let action):
            self = .toolbar(action)
        case .load(let path):
            self = .load(path: path)
        case .record(let action):
            self = .record(action)
        case .run:
            throw CLIError.invalidArgument("command is not an agent request")
        }
    }

    var line: String {
        switch self {
        case .status:
            "status"
        case .press(let buttonMask, let durationMilliseconds):
            "press \(buttonMask) \(durationMilliseconds)"
        case .button(let buttonMask, let isDown):
            "button \(buttonMask) \(isDown ? 1 : 0)"
        case .crank(let degrees):
            "crank \(Self.format(degrees))"
        case .crankDock(let isDocked):
            "crank-docked \(isDocked ? 1 : 0)"
        case .accelerometer(let x, let y, let z):
            "accelerometer \(Self.format(x)) \(Self.format(y)) \(Self.format(z))"
        case .lock:
            "lock"
        case .pause(let isPaused):
            "pause \(isPaused ? 1 : 0)"
        case .restart:
            "restart"
        case .volume(let action):
            action.protocolLine
        case .screenshot(let path):
            "screenshot \(path)"
        case .toolbar(let action):
            "toolbar \(action.rawValue)"
        case .load(let path):
            "load \(path)"
        case .setActivePDX(let path):
            "set-active-pdx \(path)"
        case .record(let action):
            action.protocolLine
        }
    }

    private static func format(_ value: Float) -> String {
        String(value)
    }
}
