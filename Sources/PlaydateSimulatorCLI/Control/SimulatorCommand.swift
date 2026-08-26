enum SimulatorCommand: Equatable, Sendable {
    case status
    case inject
    case press(button: Button, durationMilliseconds: Int)
    case button(Button, ButtonState)
    case crank(Float)
    case crankDock(CrankDockState)
    case accelerometer(x: Float, y: Float, z: Float)
    case lock
    case pause
    case resume
    case restart
    case volume(VolumeAction)
    case screenshot(path: String)
    case toolbar(ToolbarAction)
    case load(path: String)
    case record(RecordingAction)
    case run(ProjectRun)
}
