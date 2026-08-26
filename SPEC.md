# playdate-cli specification

## Scope

`playdate-simctl` gives coding agents command-line control of the macOS Playdate
Simulator. The primary workflow builds a project, launches Simulator, injects
the control agent when needed, and loads the PDX:

```text
playdate-simctl run <product.pdx> [--project-directory <path>] [--build-task <task>]
```

Other commands control loading, input, Simulator state and toolbar actions,
volume, screenshots, and GIF recording. `status` and `inject` expose agent
diagnostics. Every executable leaf command accepts `--pid`, `--agent`, and
`--simulator-app`.

## Behavior

`run` defaults to the current directory and the mise task `build`. It resolves
the PDX relative to the project directory, runs `mise run <task>`, requires a
`.pdx` directory bundle, and opens it in the selected Simulator. A cold launch
registers the active PDX; a running Simulator receives one `load` request.

`load` requires an existing `.pdx` directory bundle. `restart` reloads the PDX
established by `run` or `load`, falling back to Simulator's native restart when
none is registered. `toolbar restart` always uses the native action.

Crank positions are finite values from 0 through 360. Accelerometer components
are finite signed values. `press lock` is equivalent to `lock` and does not
accept `--duration-ms`.

Screenshot and recording destinations must use the expected extension and must
not exist. Agent requests reject line breaks and content beyond the protocol
limit.

## Design

The Swift 6.3 CLI uses swift-argument-parser and swift-subprocess. It selects
Simulator from `--simulator-app`, `PLAYDATE_SDK_PATH`, or
`~/Developer/PlaydateSDK`, in that order.

Before dispatch, the controller verifies the selected process with
`proc_pidpath` and checks the agent's protocol, PID, and capabilities. Injection
uses `nm` to verify required private symbols and LLDB to load the adjacent agent
library. A missing or refused socket triggers injection and one retry; other
transport or compatibility failures remain visible.

The injected C agent serves newline-delimited requests over a private,
process-specific Unix socket. `PlaydateSimulatorProtocol.h` is the shared
protocol-version-1 contract and limits lines to 510 bytes. The client verifies
the socket peer PID.

Simulator mutations use a bounded main-queue handoff. Delayed button releases
cannot override newer input. Screenshots produce 400 by 240 grayscale PNGs. GIF
recording runs on a dedicated thread, coalesces unchanged frames, and has no
fixed duration cap.

Captures are published without overwriting existing destinations. A failed
publication preserves the completed temporary file and reports its recovery
path.

## Safety

The CLI depends on private Simulator symbols and requires debugger attachment.
It fails closed when process identity, required symbols, protocol compatibility,
framing, or peer identity cannot be verified. It does not modify the Simulator
app or its code signature, hardcode symbol addresses, overwrite captures, or
control a physical Playdate.

## Validation

Code changes must pass swift-format, `swift test --parallel`, and
`mise run build`. Changes to private Simulator behavior, injection, or capture
also require live Simulator validation.
