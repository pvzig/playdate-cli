import ArgumentParser

enum ButtonState: String, CaseIterable, ExpressibleByArgument, Sendable {
    case down
    case up
}
