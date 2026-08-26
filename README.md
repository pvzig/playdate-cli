# playdate-cli

`playdate-simctl` gives coding agents a command-line control surface for the
macOS Playdate Simulator. An agent can build a project, load its PDX, send every
Playdate input, operate the useful Simulator toolbar actions, and capture the
screen without coordinate-based clicks or Accessibility automation.

## Prerequisite

Install the [Playdate SDK for macOS](https://play.date/dev/) before using
`playdate-simctl`. Download it from Panic and run the installer; the SDK includes
Playdate Simulator. The default installation is discovered automatically. For a
custom location, set `PLAYDATE_SDK_PATH` or pass `--simulator-app`.

> [!WARNING]
> The input bridge uses private symbols exported by Playdate Simulator and
> injects a small dynamic library with LLDB. It can break after an SDK update.
> Last validated against Playdate SDK 3.1.1.
> The CLI verifies the selected executable and agent compatibility before every
> command, verifies the socket peer on every request, and never modifies the
> Simulator app on disk.

## Run a project

From a Playdate project, the agent runs one command:

```sh
playdate-simctl run .build/path/to/YourProduct.pdx
```

That command automatically:

1. runs the project's `mise run build` task;
2. locates or launches Playdate Simulator;
3. verifies the selected Simulator and control-agent compatibility;
4. injects the control agent when it is not already loaded; and
5. loads the newly built PDX.

When invoking it from outside the project or when the build task has a
non-default name:

```sh
playdate-simctl run output/YourProduct.pdx \
  --project-directory /path/to/project \
  --build-task build-pdx
```

`run` targets Playdate Simulator, not a physical Playdate. The project only
needs a terminating mise build task that produces the specified PDX.

## Drive Simulator

After `run` returns, the agent can drive Simulator entirely from the shell. All
control commands preflight the running agent and inject it automatically if
needed. Because an injected dynamic library cannot be unloaded, an incompatible
protocol version requires restarting Simulator.

```sh
playdate-simctl status

playdate-simctl press a
playdate-simctl press b
playdate-simctl press left
playdate-simctl press right
playdate-simctl press up
playdate-simctl press down
playdate-simctl press menu
playdate-simctl press lock

playdate-simctl crank 90
playdate-simctl crank dock
playdate-simctl crank undock
playdate-simctl accelerometer 0 0 -1
playdate-simctl volume up
playdate-simctl volume down --step 25
playdate-simctl volume set 50

playdate-simctl pause
playdate-simctl resume
playdate-simctl lock
playdate-simctl restart
playdate-simctl screenshot /tmp/playdate-screen.png
playdate-simctl record start /tmp/playdate-run.gif
playdate-simctl record stop
```

`status` reports the selected Simulator PID, protocol version, and available
capabilities. `lock` toggles Simulator's lock state and reports the
resulting state; `press lock` is an alias for `lock` and does not accept
`--duration-ms`.

For deterministic holds and simultaneous inputs, use the lower-level button
form:

```sh
playdate-simctl button left down
playdate-simctl button a down
playdate-simctl button a up
playdate-simctl button left up
```

Crank positions are degrees from 0 through 360. Volume values and steps are
percentages from 0 through 100. Screenshots and recordings capture the 400×240
framebuffer and never overwrite an existing path. Agent-native GIF recording
samples at up to 50 Hz, coalesces unchanged frames with their measured delay,
and streams through a dependency-free GIF89a/LZW encoder with no duration cap.
`record stop` synchronously saves the finished file before it returns.

## Simulator toolbar

The screenshot's toolbar controls map to these CLI commands:

| Toolbar control | CLI command |
| --- | --- |
| Pause | `pause`, `resume`, or `toolbar pause` to toggle |
| Restart | `restart` or `toolbar restart` |
| Console | `toolbar console` |
| Sampler | `toolbar sampler` |
| Lua Memory | `toolbar lua-memory` |
| Screenshot | `screenshot <output.png>` |
| GIF recording | `toolbar gif` |
| Connected Device menu | `toolbar device` |
| Accelerometer & Crank drawer | `toolbar controls` |

`toolbar gif` mirrors the Simulator button exactly: the first call begins
recording and the second stops it and opens Simulator's Save Recording dialog.
Use `record start` and `record stop` for a fully non-interactive GIF. The
connected-device menu is disabled when no physical Playdate is attached.
`restart` reloads the PDX most recently established by `run` or `load`;
`toolbar restart` invokes Simulator's native restart action.

Use `--pid <pid>` on a command when more than one Simulator is running. Use
`--agent <path>` or `--simulator-app <path>` to override discovery. By default,
the Simulator app is selected from `PLAYDATE_SDK_PATH`, falling back to
`~/Developer/PlaydateSDK`. Run
`playdate-simctl --help` for the command summary or
`playdate-simctl <command> --help` for command-specific options. `load` and
`inject` exist as diagnostic primitives; neither is part of the normal
workflow.
