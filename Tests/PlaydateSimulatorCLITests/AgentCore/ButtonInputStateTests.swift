import AgentCore
import SocketSupport
import Testing

@Suite("Button input state")
struct ButtonInputStateTests {
    @Test("A later intentional input invalidates an older press release")
    func invalidatesOlderRelease() {
        let firstGeneration = pdsim_begin_button_input(PDSIM_BUTTON_A_MASK)
        #expect(pdsim_is_current_button_input(PDSIM_BUTTON_A_MASK, firstGeneration))

        let heldGeneration = pdsim_begin_button_input(PDSIM_BUTTON_A_MASK)
        #expect(pdsim_is_current_button_input(PDSIM_BUTTON_A_MASK, firstGeneration) == false)
        #expect(pdsim_is_current_button_input(PDSIM_BUTTON_A_MASK, heldGeneration))
    }
}
