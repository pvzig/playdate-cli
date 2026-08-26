import ArgumentParser

struct Record: ParsableCommand {
  static let configuration = CommandConfiguration(
    abstract: "Capture a non-interactive GIF recording.",
    subcommands: [StartRecording.self, StopRecording.self]
  )
}
