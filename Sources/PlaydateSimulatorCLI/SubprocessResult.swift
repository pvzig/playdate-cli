struct SubprocessResult: Sendable {
  let succeeded: Bool
  let output: String
  let outputWasTruncated: Bool

  init(
    succeeded: Bool,
    output: String,
    outputWasTruncated: Bool = false
  ) {
    self.succeeded = succeeded
    self.output = output
    self.outputWasTruncated = outputWasTruncated
  }
}
