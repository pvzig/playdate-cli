import ArgumentParser

struct Accelerometer: SimulatorControlCommand {
    static let configuration = CommandConfiguration(
        abstract: "Set the Simulator accelerometer vector."
    )

    @OptionGroup var simulatorOptions: SimulatorOptions

    @Argument(
        parsing: .allUnrecognized,
        help: "The x-, y-, and z-axis values."
    )
    var values: [Float]

    var simulatorCommand: SimulatorCommand {
        get throws {
            guard values.count == 3 else {
                throw ValidationError("accelerometer requires exactly three axis values")
            }
            let x = values[0]
            let y = values[1]
            let z = values[2]
            guard x.isFinite, y.isFinite, z.isFinite else {
                throw ValidationError("accelerometer values must be finite numbers")
            }
            return .accelerometer(x: x, y: y, z: z)
        }
    }
}
