import Darwin
import Foundation
import Testing

@testable import PlaydateSimulatorCLI

@Suite("Simulator controller")
struct SimulatorControllerTests {
  @Test("Preflights agent compatibility before dispatch", .timeLimit(.minutes(1)))
  func preflightsAgentCompatibility() async throws {
    let installation = SimulatorInstallation(overridePath: nil)
    let subprocessScenario = SubprocessScenario(
      processDiscoveryOutputs: ["\(getpid())\n"]
    )
    let agentRecorder = AgentRequestRecorder(commandResponse: "ok paused=1")
    let controller = makeController(
      installation: installation,
      subprocessScenario: subprocessScenario,
      agentRecorder: agentRecorder
    )

    let response = try await controller.run(
      CLIInvocation(
        processIdentifier: nil,
        agentPath: nil,
        simulatorAppPath: nil,
        command: .pause
      )
    )

    #expect(response == "ok paused=1")
    #expect(await agentRecorder.recordedRequests() == [.status, .pause(isPaused: true)])
  }

  @Test("Verifies the selected installation before contacting an existing agent")
  func verifiesInstallationBeforeExistingAgent() async {
    let installation = SimulatorInstallation(
      overridePath: "/Applications/Selected/Playdate Simulator.app"
    )
    let subprocessScenario = SubprocessScenario(
      processDiscoveryOutputs: ["\(getpid())\n"]
    )
    let agentRecorder = AgentRequestRecorder(commandResponse: "ok paused=1")
    let controller = SimulatorController(
      agentClient: AgentClient { request, processIdentifier in
        await agentRecorder.send(request, processIdentifier: processIdentifier)
      },
      processLocator: SimulatorProcessLocator(
        subprocessRunner: makeSubprocessRunner(subprocessScenario),
        executablePathProvider: { _ in
          "/Applications/Other/Playdate Simulator.app/Contents/MacOS/Playdate Simulator"
        }
      ),
      projectRunner: ProjectRunner(
        subprocessRunner: makeSubprocessRunner(subprocessScenario)
      )
    )

    do {
      _ = try await controller.run(
        CLIInvocation(
          processIdentifier: nil,
          agentPath: nil,
          simulatorAppPath: installation.appURL.path,
          command: .pause
        )
      )
      Issue.record("Expected the selected installation mismatch to be rejected")
    } catch CLIError.simulatorUnavailable {
      #expect(await agentRecorder.recordedRequests().isEmpty)
    } catch {
      Issue.record("Unexpected error: \(error)")
    }
  }

  @Test("A cold run does not reload the PDX after launch", .timeLimit(.minutes(1)))
  func coldRunLoadsProductOnce() async throws {
    let fixture = try ProjectRunFixture()
    defer { fixture.remove() }

    let installation = SimulatorInstallation(overridePath: nil)
    let subprocessScenario = SubprocessScenario(
      processDiscoveryOutputs: ["", "\(getpid())\n"]
    )
    let agentRecorder = AgentRequestRecorder(commandResponse: "ok")
    let controller = makeController(
      installation: installation,
      subprocessScenario: subprocessScenario,
      agentRecorder: agentRecorder
    )

    let response = try await controller.run(
      CLIInvocation(
        processIdentifier: nil,
        agentPath: nil,
        simulatorAppPath: nil,
        command: .run(fixture.projectRun)
      )
    )

    #expect(response == "ok")
    #expect(
      await agentRecorder.recordedRequests()
        == [.status, .setActivePDX(path: fixture.projectRun.productPath)]
    )
  }

  @Test("A warm run reloads the PDX exactly once", .timeLimit(.minutes(1)))
  func warmRunReloadsProductOnce() async throws {
    let fixture = try ProjectRunFixture()
    defer { fixture.remove() }

    let installation = SimulatorInstallation(overridePath: nil)
    let subprocessScenario = SubprocessScenario(
      processDiscoveryOutputs: ["\(getpid())\n"]
    )
    let agentRecorder = AgentRequestRecorder(commandResponse: "ok")
    let controller = makeController(
      installation: installation,
      subprocessScenario: subprocessScenario,
      agentRecorder: agentRecorder
    )

    let response = try await controller.run(
      CLIInvocation(
        processIdentifier: nil,
        agentPath: nil,
        simulatorAppPath: nil,
        command: .run(fixture.projectRun)
      )
    )

    #expect(response == "ok")
    #expect(
      await agentRecorder.recordedRequests()
        == [.status, .load(path: fixture.projectRun.productPath)]
    )
  }

  private func makeController(
    installation: SimulatorInstallation,
    subprocessScenario: SubprocessScenario,
    agentRecorder: AgentRequestRecorder
  ) -> SimulatorController {
    let subprocessRunner = makeSubprocessRunner(subprocessScenario)
    return SimulatorController(
      agentClient: AgentClient { request, processIdentifier in
        await agentRecorder.send(request, processIdentifier: processIdentifier)
      },
      agentInjector: AgentInjector(subprocessRunner: subprocessRunner),
      binaryVerifier: SimulatorBinaryVerifier(subprocessRunner: subprocessRunner),
      processLocator: SimulatorProcessLocator(
        subprocessRunner: subprocessRunner,
        executablePathProvider: { _ in installation.executableURL.path }
      ),
      projectRunner: ProjectRunner(subprocessRunner: subprocessRunner)
    )
  }
}

private func makeSubprocessRunner(_ scenario: SubprocessScenario) -> SubprocessRunner {
  SubprocessRunner { executable, arguments, workingDirectory, outputLimit in
    try await scenario.run(
      executable: executable,
      arguments: arguments,
      workingDirectory: workingDirectory,
      outputLimit: outputLimit
    )
  }
}

private actor AgentRequestRecorder {
  private let commandResponse: String
  private var requests: [AgentRequest] = []

  init(commandResponse: String) {
    self.commandResponse = commandResponse
  }

  func send(_ request: AgentRequest, processIdentifier: Int32) -> String {
    requests.append(request)
    if request == .status {
      return Self.statusResponse(processIdentifier: processIdentifier)
    }
    return commandResponse
  }

  func recordedRequests() -> [AgentRequest] {
    requests
  }

  private static func statusResponse(processIdentifier: Int32) -> String {
    "ok protocol=1 pid=\(processIdentifier) buttons=1 crank=1 dock=1 "
      + "accelerometer=1 lock=1 volume=1 ui=1 screenshot=1 load=1 record=1"
  }
}

private actor SubprocessScenario {
  private var processDiscoveryOutputs: [String]

  init(processDiscoveryOutputs: [String]) {
    self.processDiscoveryOutputs = processDiscoveryOutputs
  }

  func run(
    executable: String,
    arguments: [String],
    workingDirectory: String?,
    outputLimit: Int
  ) throws -> SubprocessResult {
    _ = arguments
    _ = workingDirectory
    _ = outputLimit

    switch executable {
    case "/usr/bin/pgrep":
      guard processDiscoveryOutputs.isEmpty == false else {
        throw CLIError.subprocessFailed("unexpected extra process discovery")
      }
      return SubprocessResult(
        succeeded: true,
        output: processDiscoveryOutputs.removeFirst()
      )
    case "/usr/bin/env", "/usr/bin/open":
      return SubprocessResult(succeeded: true, output: "")
    default:
      throw CLIError.subprocessFailed("unexpected subprocess: \(executable)")
    }
  }
}

private struct ProjectRunFixture {
  let rootURL: URL
  let projectRun: ProjectRun

  init() throws {
    rootURL = FileManager.default.temporaryDirectory
      .appending(path: "playdate-simctl-run-test-\(UUID().uuidString)")
    let productURL = rootURL.appending(path: "Build/Game.pdx", directoryHint: .isDirectory)
    try FileManager.default.createDirectory(
      at: productURL,
      withIntermediateDirectories: true
    )
    projectRun = ProjectRun(
      projectDirectory: rootURL.path,
      productPath: productURL.path,
      buildTask: "build"
    )
  }

  func remove() {
    try? FileManager.default.removeItem(at: rootURL)
  }
}
