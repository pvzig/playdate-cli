import Darwin
import DescriptorIO
import Testing

@Suite("Descriptor I/O")
struct DescriptorIOTests {
  @Test("Writes and reads one newline-delimited line")
  func readsCompleteLine() throws {
    let descriptors = try makePipe()
    defer {
      close(descriptors.read)
      close(descriptors.write)
    }

    let writeResult = "status".withCString { bytes in
      unsafe pdsim_write_line(descriptors.write, bytes, strlen(bytes))
    }
    try #require(writeResult == 0)

    var buffer = [CChar](repeating: 0, count: 16)
    var lineLength = 0
    let readResult = unsafe pdsim_read_line(
      descriptors.read,
      &buffer,
      buffer.count,
      &lineLength
    )

    #expect(readResult == pdsim_line_read_success)
    #expect(decode(buffer, length: lineLength) == "status")
  }

  @Test("Distinguishes a line that exceeds the buffer")
  func rejectsOversizedLine() throws {
    let descriptors = try makePipe()
    defer {
      close(descriptors.read)
      close(descriptors.write)
    }

    let writeResult = "12345".withCString { bytes in
      unsafe pdsim_write_line(descriptors.write, bytes, strlen(bytes))
    }
    try #require(writeResult == 0)

    var buffer = [CChar](repeating: 0, count: 5)
    var lineLength = 0
    let readResult = unsafe pdsim_read_line(
      descriptors.read,
      &buffer,
      buffer.count,
      &lineLength
    )

    #expect(readResult == pdsim_line_read_too_long)
    #expect(lineLength == buffer.count - 1)
    #expect(decode(buffer, length: lineLength) == "1234")
  }

  @Test("Distinguishes end of file before a newline")
  func rejectsUnterminatedLine() throws {
    let descriptors = try makePipe()
    defer { close(descriptors.read) }

    let writeResult = "ok".withCString { bytes in
      unsafe pdsim_write_all(descriptors.write, bytes, strlen(bytes))
    }
    try #require(writeResult == 0)
    try #require(close(descriptors.write) == 0)

    var buffer = [CChar](repeating: 0, count: 16)
    var lineLength = 0
    let readResult = unsafe pdsim_read_line(
      descriptors.read,
      &buffer,
      buffer.count,
      &lineLength
    )

    #expect(readResult == pdsim_line_read_end_of_file)
    #expect(decode(buffer, length: lineLength) == "ok")
  }

  private func makePipe() throws -> (read: Int32, write: Int32) {
    var descriptors = [Int32](repeating: -1, count: 2)
    try #require(pipe(&descriptors) == 0)
    return (descriptors[0], descriptors[1])
  }

  private func decode(_ buffer: [CChar], length: Int) -> String {
    String(
      decoding: buffer.prefix(length).map { UInt8(bitPattern: $0) },
      as: UTF8.self
    )
  }
}
