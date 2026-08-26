import AgentCore
import Testing

@Suite("Agent command parser")
struct CommandParserTests {
    @Test("Parses every command kind")
    func parsesSupportedCommands() {
        let commands: [(line: String, kind: pdsim_agent_command_kind)] = [
            ("status", pdsim_agent_command_status),
            ("button 32 1", pdsim_agent_command_button),
            ("press 16 125", pdsim_agent_command_press),
            ("crank 90", pdsim_agent_command_crank),
            ("crank-docked 1", pdsim_agent_command_crank_docked),
            ("accelerometer 0.25 -0.5 1", pdsim_agent_command_accelerometer),
            ("lock", pdsim_agent_command_lock),
            ("load /tmp/Example.pdx", pdsim_agent_command_load),
            ("set-active-pdx /tmp/Example.pdx", pdsim_agent_command_set_active_pdx),
            ("pause 1", pdsim_agent_command_pause),
            ("restart", pdsim_agent_command_restart),
            ("volume-adjust -25", pdsim_agent_command_volume_adjust),
            ("volume-set 50", pdsim_agent_command_volume_set),
            ("screenshot /tmp/screen.png", pdsim_agent_command_screenshot),
            ("toolbar lua-memory", pdsim_agent_command_toolbar),
            ("record-start /tmp/recording.gif", pdsim_agent_command_record_start),
            ("record-stop", pdsim_agent_command_record_stop),
        ]

        for command in commands {
            withParsedCommand(command.line) { result, parsedCommand in
                #expect(result == pdsim_agent_command_parse_success)
                #expect(parsedCommand.kind == command.kind)
            }
        }
    }

    @Test("Preserves parsed values and borrowed paths")
    func preservesValues() {
        withParsedCommand("press 64 10000") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.button == 64)
            #expect(command.duration_milliseconds == 10_000)
        }

        withParsedCommand("accelerometer 0.25 -0.5 1") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.x == 0.25)
            #expect(command.y == -0.5)
            #expect(command.z == 1)
        }

        withParsedCommand("load /tmp/My Game.pdx") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.path.map(String.init(cString:)) == "/tmp/My Game.pdx")
        }

        withParsedCommand("set-active-pdx /tmp/My Game.pdx") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.path.map(String.init(cString:)) == "/tmp/My Game.pdx")
        }

        withParsedCommand("record-start /tmp/My Recording.gif") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.path.map(String.init(cString:)) == "/tmp/My Recording.gif")
        }

        withParsedCommand("toolbar controls") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.toolbar_action == pdsim_toolbar_controls)
        }
    }

    @Test("Rejects invalid command grammar with stable diagnostics")
    func rejectsInvalidCommands() {
        let invalidCommands:
            [(line: String, result: pdsim_agent_command_parse_result, message: String)] = [
                (
                    "button 3 1", pdsim_agent_command_parse_invalid_button,
                    "error invalid button command"
                ),
                (
                    "press 32 0", pdsim_agent_command_parse_invalid_press,
                    "error invalid press command"
                ),
                (
                    "press 32 10001", pdsim_agent_command_parse_invalid_press,
                    "error invalid press command"
                ),
                (
                    "crank 361", pdsim_agent_command_parse_invalid_crank,
                    "error invalid crank command"
                ),
                (
                    "crank-docked 2",
                    pdsim_agent_command_parse_invalid_crank_docked,
                    "error invalid crank-docked command"
                ),
                (
                    "accelerometer 0 nan 1",
                    pdsim_agent_command_parse_invalid_accelerometer,
                    "error invalid accelerometer command"
                ),
                (
                    "load relative.pdx",
                    pdsim_agent_command_parse_invalid_load,
                    "error load requires an absolute .pdx path"
                ),
                (
                    "set-active-pdx relative.pdx",
                    pdsim_agent_command_parse_invalid_active_pdx,
                    "error active PDX requires an absolute .pdx path"
                ),
                ("pause 2", pdsim_agent_command_parse_invalid_pause, "error invalid pause command"),
                (
                    "volume-adjust 0",
                    pdsim_agent_command_parse_invalid_volume,
                    "error invalid volume command"
                ),
                (
                    "screenshot relative.png",
                    pdsim_agent_command_parse_invalid_screenshot,
                    "error screenshot path must be absolute"
                ),
                (
                    "toolbar unknown",
                    pdsim_agent_command_parse_invalid_toolbar,
                    "error invalid toolbar command"
                ),
                ("unknown", pdsim_agent_command_parse_unknown, "error unknown command"),
            ]

        for invalidCommand in invalidCommands {
            withParsedCommand(invalidCommand.line) { result, _ in
                #expect(result == invalidCommand.result)
                #expect(
                    pdsim_agent_command_parse_error(result).map(String.init(cString:))
                        == invalidCommand.message
                )
            }
        }
    }

    @Test("Uses period decimal syntax independent of the process locale")
    func usesProtocolDecimalSyntax() {
        withParsedCommand("crank 12.5") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.x == 12.5)
        }
        withParsedCommand("accelerometer 0,25 -0.5 1") { result, _ in
            #expect(result == pdsim_agent_command_parse_invalid_accelerometer)
        }
        withParsedCommand("accelerometer 1e-45 -1e-45 0") { result, command in
            #expect(result == pdsim_agent_command_parse_success)
            #expect(command.x > 0)
            #expect(command.y < 0)
        }
    }

    @Test(
        "Rejects integer fields outside the C int range",
        arguments: [
            ("button 4294967328 1", pdsim_agent_command_parse_invalid_button),
            ("press 32 4294977296", pdsim_agent_command_parse_invalid_press),
            ("crank-docked 4294967296", pdsim_agent_command_parse_invalid_crank_docked),
            ("pause 4294967297", pdsim_agent_command_parse_invalid_pause),
            ("volume-adjust 4294967297", pdsim_agent_command_parse_invalid_volume),
            ("volume-set 4294967346", pdsim_agent_command_parse_invalid_volume),
        ]
    )
    func rejectsOverflowingInteger(
        line: String,
        expectedResult: pdsim_agent_command_parse_result
    ) {
        withParsedCommand(line) { result, _ in
            #expect(result == expectedResult)
        }
    }

    private func withParsedCommand(
        _ line: String,
        assertions: (
            pdsim_agent_command_parse_result,
            pdsim_agent_command
        ) -> Void
    ) {
        line.withCString { request in
            var command = pdsim_agent_command()
            let result = pdsim_parse_agent_command(request, &command)
            assertions(result, command)
        }
    }
}
