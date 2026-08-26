import Testing

@testable import PlaydateSimulatorCLI

@Suite("Button")
struct ButtonTests {
  @Test(
    "Maps buttons to PDButtons masks",
    arguments: [
      (Button.left, Int32(1)),
      (.right, Int32(2)),
      (.up, Int32(4)),
      (.down, Int32(8)),
      (.b, Int32(16)),
      (.a, Int32(32)),
      (.menu, Int32(64)),
    ]
  )
  func mapsToMask(button: Button, expectedMask: Int32) {
    #expect(button.mask == expectedMask)
  }
}
