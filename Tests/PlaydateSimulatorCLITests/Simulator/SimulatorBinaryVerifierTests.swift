import Testing

@testable import PlaydateSimulatorCLI

@Suite("Simulator binary verifier")
struct SimulatorBinaryVerifierTests {
    @Test("Loads the required symbols from the shared protocol contract")
    func loadsRequiredSymbols() {
        #expect(SimulatorBinaryVerifier.requiredSymbols.count == 20)
        #expect(SimulatorBinaryVerifier.requiredSymbols.contains("sim_handleButton"))
        #expect(SimulatorBinaryVerifier.requiredSymbols.contains("__ZL5frame"))
        #expect(
            SimulatorBinaryVerifier.requiredSymbols.contains(
                "_ZN9MainFrame22OnToggleDeviceControlsER14wxCommandEvent"
            )
        )
    }

    @Test("Parses nm symbols while preserving raw and normalized Mach-O names")
    func parsesDefinedSymbols() {
        let symbols = SimulatorBinaryVerifier.parseDefinedSymbols(
            """
            00000001000cd2a0 T _sim_handleButton
            00000001000cd350 T _sim_setCrankPosition
            00000001000cd400 d __ZL5frame
            malformed
            """
        )

        #expect(symbols.contains("sim_handleButton"))
        #expect(symbols.contains("sim_setCrankPosition"))
        #expect(symbols.contains("__ZL5frame"))
    }
}
