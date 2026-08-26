import CoreGraphics
import Darwin
import Foundation
import GIFEncoder
import ImageIO
import Testing

@Suite("GIF encoder")
struct GIFEncoderTests {
  private let width = 400
  private let height = 240
  private let bytesPerRow = 52

  @Test("Round-trips packed frames, timing, and row padding")
  func roundTripsFrames() throws {
    let outputURL = FileManager.default.temporaryDirectory
      .appending(path: "playdate-cli-gif-\(UUID().uuidString)")
      .appendingPathExtension("gif")
    #expect(FileManager.default.createFile(atPath: outputURL.path, contents: nil))
    defer { try? FileManager.default.removeItem(at: outputURL) }

    let file = try FileHandle(forWritingTo: outputURL)
    let encoderDescriptor = dup(file.fileDescriptor)
    try file.close()
    #expect(encoderDescriptor >= 0)

    let encoder = try #require(
      pdsim_gif_encoder_create(
        encoderDescriptor,
        UInt16(width),
        UInt16(height)
      )
    )
    defer { pdsim_gif_encoder_destroy(encoder) }

    var blackFrame = [UInt8](repeating: 0, count: bytesPerRow * height)
    for row in 0..<height {
      blackFrame[row * bytesPerRow + 50] = 0xFF
      blackFrame[row * bytesPerRow + 51] = 0xFF
    }
    var splitFrame = [UInt8](repeating: 0, count: bytesPerRow * height)
    for row in 0..<height {
      for column in (width / 2)..<width {
        splitFrame[row * bytesPerRow + column / 8] |= UInt8(0x80 >> (column % 8))
      }
    }
    var noiseFrame = [UInt8](repeating: 0, count: bytesPerRow * height)
    var randomState: UInt32 = 0xC0FFEE
    for row in 0..<height {
      for byteIndex in 0..<(width / 8) {
        randomState = 1_664_525 &* randomState &+ 1_013_904_223
        noiseFrame[row * bytesPerRow + byteIndex] = UInt8(truncatingIfNeeded: randomState >> 24)
      }
    }

    #expect(
      blackFrame.withUnsafeBytes { frame in
        pdsim_gif_encoder_add_frame(
          encoder,
          frame.baseAddress?.assumingMemoryBound(to: UInt8.self),
          bytesPerRow,
          2
        )
      }
    )
    #expect(
      splitFrame.withUnsafeBytes { frame in
        pdsim_gif_encoder_add_frame(
          encoder,
          frame.baseAddress?.assumingMemoryBound(to: UInt8.self),
          bytesPerRow,
          7
        )
      }
    )
    #expect(
      noiseFrame.withUnsafeBytes { frame in
        pdsim_gif_encoder_add_frame(
          encoder,
          frame.baseAddress?.assumingMemoryBound(to: UInt8.self),
          bytesPerRow,
          11
        )
      }
    )
    #expect(pdsim_gif_encoder_finish(encoder))

    let source = try #require(CGImageSourceCreateWithURL(outputURL as CFURL, nil))
    #expect(CGImageSourceGetCount(source) == 3)
    let fileProperties = try #require(
      CGImageSourceCopyProperties(source, nil) as? [CFString: Any]
    )
    let fileGIFProperties = try #require(
      fileProperties[kCGImagePropertyGIFDictionary] as? [CFString: Any]
    )
    #expect(fileGIFProperties[kCGImagePropertyGIFLoopCount] as? Int == 0)

    let firstImage = try #require(CGImageSourceCreateImageAtIndex(source, 0, nil))
    let secondImage = try #require(CGImageSourceCreateImageAtIndex(source, 1, nil))
    let thirdImage = try #require(CGImageSourceCreateImageAtIndex(source, 2, nil))
    #expect(firstImage.width == width)
    #expect(firstImage.height == height)

    let firstPixels = try grayscalePixels(in: firstImage)
    let secondPixels = try grayscalePixels(in: secondImage)
    let thirdPixels = try grayscalePixels(in: thirdImage)
    #expect(firstPixels[width / 4] < 16)
    #expect(firstPixels[width * 3 / 4] < 16)
    #expect(secondPixels[width / 4] < 16)
    #expect(secondPixels[width * 3 / 4] > 240)
    let expectedNoise = (0..<(width * height)).map { pixelIndex in
      let row = pixelIndex / width
      let column = pixelIndex % width
      let byte = noiseFrame[row * bytesPerRow + column / 8]
      return byte & UInt8(0x80 >> (column % 8)) != 0
    }
    #expect(thirdPixels.map { $0 > 127 } == expectedNoise)

    #expect(try frameDelay(in: source, at: 0) == 0.02)
    #expect(try frameDelay(in: source, at: 1) == 0.07)
    #expect(try frameDelay(in: source, at: 2) == 0.11)
  }

  private func grayscalePixels(in image: CGImage) throws -> [UInt8] {
    var pixels = [UInt8](repeating: 0, count: width * height)
    let drewImage = pixels.withUnsafeMutableBytes { storage in
      guard
        let context = CGContext(
          data: storage.baseAddress,
          width: width,
          height: height,
          bitsPerComponent: 8,
          bytesPerRow: width,
          space: CGColorSpaceCreateDeviceGray(),
          bitmapInfo: CGImageAlphaInfo.none.rawValue
        )
      else {
        return false
      }
      context.interpolationQuality = .none
      context.draw(
        image,
        in: CGRect(x: 0, y: 0, width: width, height: height)
      )
      return true
    }
    guard drewImage else {
      throw GIFEncoderTestError.couldNotCreateBitmapContext
    }
    return pixels
  }

  private func frameDelay(in source: CGImageSource, at index: Int) throws -> Double {
    let properties = try #require(
      CGImageSourceCopyPropertiesAtIndex(source, index, nil) as? [CFString: Any]
    )
    let gifProperties = try #require(
      properties[kCGImagePropertyGIFDictionary] as? [CFString: Any]
    )
    return try #require(
      gifProperties[kCGImagePropertyGIFUnclampedDelayTime] as? Double
        ?? gifProperties[kCGImagePropertyGIFDelayTime] as? Double
    )
  }
}

private enum GIFEncoderTestError: Error {
  case couldNotCreateBitmapContext
}
