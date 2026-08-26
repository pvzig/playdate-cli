import Foundation
import SocketSupport

struct SimulatorBinaryVerifier: Sendable {
  static let requiredSymbols: Set<String> = Set(
    (0..<Int(pdsim_required_simulator_symbol_count())).compactMap { index in
      guard let symbol = pdsim_required_simulator_symbol_at(index) else {
        return nil
      }
      return String(cString: symbol)
    }
  ).union([String(cString: pdsim_main_frame_symbol())])

  let subprocessRunner: SubprocessRunner

  init(subprocessRunner: SubprocessRunner = SubprocessRunner()) {
    self.subprocessRunner = subprocessRunner
  }

  func verify(_ installation: SimulatorInstallation) async throws {
    let executablePath = installation.executableURL.path
    guard FileManager.default.isExecutableFile(atPath: executablePath) else {
      throw CLIError.simulatorUnavailable(
        "Playdate Simulator executable was not found at \(executablePath)"
      )
    }

    let result = try await subprocessRunner.run(
      executable: "/usr/bin/nm",
      arguments: ["-jU", executablePath],
      outputLimit: 16 * 1024 * 1024
    )
    guard result.succeeded else {
      throw CLIError.subprocessFailed("nm could not inspect the Simulator: \(result.output)")
    }
    guard !result.outputWasTruncated else {
      throw CLIError.subprocessFailed("nm output exceeded the compatibility inspection limit")
    }

    let definedSymbols = Self.parseDefinedSymbols(result.output)
    let missingSymbols = Self.requiredSymbols.subtracting(definedSymbols).sorted()
    guard missingSymbols.isEmpty else {
      throw CLIError.simulatorUnavailable(
        "this Simulator does not define the required private symbols: "
          + missingSymbols.joined(separator: ", ")
      )
    }
  }

  static func parseDefinedSymbols(_ output: String) -> Set<String> {
    Set(
      output.split(separator: "\n").flatMap { line -> [String] in
        guard let symbol = line.split(whereSeparator: \.isWhitespace).last else {
          return []
        }
        let rawSymbol = String(symbol)
        guard symbol.first == "_" else {
          return [rawSymbol]
        }
        return [rawSymbol, String(symbol.dropFirst())]
      }
    )
  }
}
