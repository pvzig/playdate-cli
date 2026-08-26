import Foundation

struct SimulatorController: Sendable {
  let agentClient: AgentClient
  let agentInjector: AgentInjector
  let agentLocator: AgentLocator
  let binaryVerifier: SimulatorBinaryVerifier
  let processLocator: SimulatorProcessLocator
  let projectRunner: ProjectRunner

  init(
    agentClient: AgentClient = AgentClient(),
    agentInjector: AgentInjector = AgentInjector(),
    agentLocator: AgentLocator = AgentLocator(),
    binaryVerifier: SimulatorBinaryVerifier = SimulatorBinaryVerifier(),
    processLocator: SimulatorProcessLocator = SimulatorProcessLocator(),
    projectRunner: ProjectRunner = ProjectRunner()
  ) {
    self.agentClient = agentClient
    self.agentInjector = agentInjector
    self.agentLocator = agentLocator
    self.binaryVerifier = binaryVerifier
    self.processLocator = processLocator
    self.projectRunner = projectRunner
  }

  func run(_ invocation: CLIInvocation) async throws -> String {
    let installation = SimulatorInstallation(overridePath: invocation.simulatorAppPath)
    var launchedProductOnColdStart = false
    var preLaunchProcessIdentifier: Int32?
    if case .run(let projectRun) = invocation.command {
      try await projectRunner.build(projectRun)
      let existingProcessIdentifier = try await processLocator.resolveIfRunning(
        explicitProcessIdentifier: invocation.processIdentifier
      )
      preLaunchProcessIdentifier = existingProcessIdentifier
      if let existingProcessIdentifier {
        try await processLocator.verify(
          processIdentifier: existingProcessIdentifier,
          installation: installation
        )
      } else {
        launchedProductOnColdStart = true
      }
      try await projectRunner.launch(projectRun, installation: installation)
    }

    let processIdentifier: Int32
    if let preLaunchProcessIdentifier {
      processIdentifier = preLaunchProcessIdentifier
    } else {
      processIdentifier = try await resolveProcess(
        explicitProcessIdentifier: invocation.processIdentifier,
        waitsForLaunch: invocation.command.isProjectRun
      )
    }
    try await processLocator.verify(
      processIdentifier: processIdentifier,
      installation: installation
    )

    switch invocation.command {
    case .status, .inject:
      return try await ensureAgent(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: invocation.agentPath
      )
    case .run(let projectRun):
      _ = try await ensureAgent(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: invocation.agentPath
      )
      if launchedProductOnColdStart {
        return try await performWithAgentRecovery(
          processIdentifier: processIdentifier,
          installation: installation,
          agentPath: invocation.agentPath
        ) {
          try await agentClient.send(
            .setActivePDX(path: projectRun.productPath),
            processIdentifier: processIdentifier
          )
        }
      }
      return try await performWithAgentRecovery(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: invocation.agentPath
      ) {
        try await agentClient.send(
          .load(path: projectRun.productPath),
          processIdentifier: processIdentifier
        )
      }
    default:
      _ = try await ensureAgent(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: invocation.agentPath
      )
      return try await performWithAgentRecovery(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: invocation.agentPath
      ) {
        try await agentClient.send(
          AgentRequest(command: invocation.command),
          processIdentifier: processIdentifier
        )
      }
    }
  }

  private func performWithAgentRecovery(
    processIdentifier: Int32,
    installation: SimulatorInstallation,
    agentPath: String?,
    operation: () async throws -> String
  ) async throws -> String {
    do {
      return try await operation()
    } catch CLIError.agentNotRunning {
      _ = try await injectAgent(
        processIdentifier: processIdentifier,
        installation: installation,
        agentPath: agentPath
      )
      return try await operation()
    }
  }

  private func resolveProcess(
    explicitProcessIdentifier: Int32?,
    waitsForLaunch: Bool
  ) async throws -> Int32 {
    guard waitsForLaunch else {
      return try await processLocator.resolve(
        explicitProcessIdentifier: explicitProcessIdentifier
      )
    }

    let clock = ContinuousClock()
    var lastError: Error?
    for _ in 0..<40 {
      do {
        return try await processLocator.resolve(
          explicitProcessIdentifier: explicitProcessIdentifier
        )
      } catch {
        lastError = error
        try await clock.sleep(for: .milliseconds(50))
      }
    }
    throw lastError ?? CLIError.simulatorUnavailable("Playdate Simulator did not launch")
  }

  private func ensureAgent(
    processIdentifier: Int32,
    installation: SimulatorInstallation,
    agentPath: String?
  ) async throws -> String {
    do {
      let status = try await agentClient.send(.status, processIdentifier: processIdentifier)
      try AgentStatus.validate(
        response: status,
        expectedProcessIdentifier: processIdentifier
      )
      return status
    } catch CLIError.agentNotRunning {
      // A missing socket is the expected first-run path. Other agent
      // failures must remain visible rather than triggering reinjection.
    }

    return try await injectAgent(
      processIdentifier: processIdentifier,
      installation: installation,
      agentPath: agentPath
    )
  }

  private func injectAgent(
    processIdentifier: Int32,
    installation: SimulatorInstallation,
    agentPath: String?
  ) async throws -> String {
    try await processLocator.verify(
      processIdentifier: processIdentifier,
      installation: installation
    )
    try await binaryVerifier.verify(installation)
    let agentURL = try agentLocator.resolve(overridePath: agentPath)
    try await agentInjector.inject(
      agentURL: agentURL,
      processIdentifier: processIdentifier
    )

    let clock = ContinuousClock()
    var lastError: Error?
    for _ in 0..<40 {
      do {
        let status = try await agentClient.send(.status, processIdentifier: processIdentifier)
        try AgentStatus.validate(
          response: status,
          expectedProcessIdentifier: processIdentifier
        )
        return status
      } catch let readinessError as CLIError {
        switch readinessError {
        case .agentNotRunning, .agentNotReady, .agentConnectionFailed:
          lastError = readinessError
        default:
          throw readinessError
        }
        try await clock.sleep(for: .milliseconds(50))
      } catch {
        throw error
      }
    }

    throw CLIError.agentUnavailable(
      "the injected agent did not open its socket: "
        + (lastError?.localizedDescription ?? "unknown error")
    )
  }
}

extension SimulatorCommand {
  fileprivate var isProjectRun: Bool {
    if case .run = self {
      return true
    }
    return false
  }
}
