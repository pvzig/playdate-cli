import CaptureSupport
import Darwin
import Foundation
import SocketSupport
import Testing

@Suite("Capture files")
struct CaptureFileTests {
  enum CaptureKind: CaseIterable, Sendable {
    case recording
    case screenshot

    var recoveryMessagePrefix: String {
      switch self {
      case .recording:
        "error could not save GIF recording; temporary file preserved at "
      case .screenshot:
        "error could not save screenshot; temporary file preserved at "
      }
    }
  }

  @Test("Creates reportable, type-specific temporary paths", arguments: CaptureKind.allCases)
  func createsReportableTemporaryPath(kind: CaptureKind) throws {
    var path = [CChar](
      repeating: 0,
      count: Int(PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY)
    )
    let descriptor =
      switch kind {
      case .recording:
        pdsim_create_recording_temporary_file(&path, path.count)
      case .screenshot:
        pdsim_create_screenshot_temporary_file(&path, path.count)
      }
    try #require(descriptor >= 0)

    let temporaryPath = decodeCString(path)
    defer {
      close(descriptor)
      unlink(temporaryPath)
    }

    #expect(temporaryPath.hasPrefix("/tmp/playdate-simctl-"))
    #expect(temporaryPath.hasSuffix(kind == .recording ? ".gif" : ".png"))
    let response = "\(kind.recoveryMessagePrefix)\(temporaryPath): File exists"
    #expect(response.utf8.count <= Int(PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES))
  }

  @Test("Preserves the reportable temporary file when a long destination appears")
  func preservesTemporaryFileForLongDestinationCollision() throws {
    let fileManager = FileManager.default
    let testDirectory = URL(filePath: "/tmp")
      .appending(path: "playdate-simctl-capture-test-\(UUID().uuidString)")
    var outputDirectory = testDirectory
    for index in 0..<4 {
      outputDirectory.append(
        path: "\(index)-\(String(repeating: "a", count: 80))"
      )
    }
    try fileManager.createDirectory(
      at: outputDirectory,
      withIntermediateDirectories: true
    )
    defer { try? fileManager.removeItem(at: testDirectory) }

    let outputURL = outputDirectory.appending(path: "capture.png")
    try Data("existing".utf8).write(to: outputURL)
    #expect("screenshot \(outputURL.path)".utf8.count <= Int(PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES))

    var temporaryPathStorage = [CChar](
      repeating: 0,
      count: Int(PDSIM_CAPTURE_TEMPORARY_PATH_CAPACITY)
    )
    let descriptor = pdsim_create_screenshot_temporary_file(
      &temporaryPathStorage,
      temporaryPathStorage.count
    )
    try #require(descriptor >= 0)
    close(descriptor)
    let temporaryPath = decodeCString(temporaryPathStorage)
    defer { unlink(temporaryPath) }
    try Data("capture".utf8).write(to: URL(filePath: temporaryPath))

    var saveError: Int32 = 0
    let published = temporaryPath.withCString { temporaryPathPointer in
      outputURL.path.withCString { outputPathPointer in
        pdsim_publish_capture_temporary_file(
          temporaryPathPointer,
          outputPathPointer,
          &saveError
        )
      }
    }

    #expect(published == false)
    #expect(saveError == EEXIST)
    #expect(try Data(contentsOf: outputURL) == Data("existing".utf8))
    #expect(try Data(contentsOf: URL(filePath: temporaryPath)) == Data("capture".utf8))
    let saveErrorDescription = strerror(saveError).map { String(cString: $0) } ?? "unknown error"
    let response =
      "error could not save screenshot; temporary file preserved at \(temporaryPath): \(saveErrorDescription)"
    #expect(response.contains(temporaryPath))
    #expect(response.utf8.count <= Int(PDSIM_PROTOCOL_MAXIMUM_LINE_BYTES))
  }

  @Test("Publishes through the exclusive-copy fallback without overwriting")
  func publishesThroughExclusiveCopy() throws {
    let fileManager = FileManager.default
    let testDirectory = fileManager.temporaryDirectory
      .appending(path: "playdate-simctl-copy-test-\(UUID().uuidString)")
    try fileManager.createDirectory(at: testDirectory, withIntermediateDirectories: false)
    defer { try? fileManager.removeItem(at: testDirectory) }

    let temporaryURL = testDirectory.appending(path: "capture.tmp")
    let outputURL = testDirectory.appending(path: "capture.png")
    let capture = Data("capture".utf8)
    try capture.write(to: temporaryURL)

    var saveError: Int32 = 0
    let published = temporaryURL.path.withCString { temporaryPathPointer in
      outputURL.path.withCString { outputPathPointer in
        pdsim_publish_capture_by_exclusive_copy(
          temporaryPathPointer,
          outputPathPointer,
          &saveError
        )
      }
    }

    #expect(published)
    #expect(fileManager.fileExists(atPath: temporaryURL.path) == false)
    #expect(try Data(contentsOf: outputURL) == capture)

    try Data("second".utf8).write(to: temporaryURL)
    let overwritten = temporaryURL.path.withCString { temporaryPathPointer in
      outputURL.path.withCString { outputPathPointer in
        pdsim_publish_capture_by_exclusive_copy(
          temporaryPathPointer,
          outputPathPointer,
          &saveError
        )
      }
    }
    #expect(overwritten == false)
    #expect(saveError == EEXIST)
    #expect(try Data(contentsOf: outputURL) == capture)
    #expect(fileManager.fileExists(atPath: temporaryURL.path))
  }

  private func decodeCString(_ characters: [CChar]) -> String {
    String(
      decoding: characters.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) },
      as: UTF8.self
    )
  }
}
