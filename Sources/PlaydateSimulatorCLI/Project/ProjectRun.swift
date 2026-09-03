import Foundation

struct ProjectRun: Equatable, Sendable {
    let projectDirectoryURL: URL
    let productURL: URL
    let buildTask: String
}
