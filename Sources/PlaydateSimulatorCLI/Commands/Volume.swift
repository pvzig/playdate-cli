import ArgumentParser

struct Volume: ParsableCommand {
  static let configuration = CommandConfiguration(
    abstract: "Adjust or set the Simulator volume.",
    subcommands: [VolumeUp.self, VolumeDown.self, VolumeSet.self]
  )
}
