import ArgumentParser

enum ToolbarAction: String, CaseIterable, ExpressibleByArgument, Sendable {
  case pause
  case restart
  case console
  case sampler
  case memory = "lua-memory"
  case record = "gif"
  case device
  case controls
}
