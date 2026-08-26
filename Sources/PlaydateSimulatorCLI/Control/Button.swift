import ArgumentParser
import SocketSupport

enum Button: String, CaseIterable, ExpressibleByArgument, Sendable {
    case left
    case right
    case up
    case down
    case b
    case a
    case menu
    var mask: Int32 {
        switch self {
        case .left:
            Int32(PDSIM_BUTTON_LEFT_MASK)
        case .right:
            Int32(PDSIM_BUTTON_RIGHT_MASK)
        case .up:
            Int32(PDSIM_BUTTON_UP_MASK)
        case .down:
            Int32(PDSIM_BUTTON_DOWN_MASK)
        case .b:
            Int32(PDSIM_BUTTON_B_MASK)
        case .a:
            Int32(PDSIM_BUTTON_A_MASK)
        case .menu:
            Int32(PDSIM_BUTTON_MENU_MASK)
        }
    }
}
