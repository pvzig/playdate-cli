import Subprocess
import System

struct SubprocessRunner: Sendable {
    typealias Operation =
        @Sendable (
            _ executable: String,
            _ arguments: [String],
            _ workingDirectory: String?,
            _ outputLimit: Int
        ) async throws -> SubprocessResult

    private let operation: Operation?

    init(operation: Operation? = nil) {
        self.operation = operation
    }

    func run(
        executable: String,
        arguments: [String],
        workingDirectory: String? = nil,
        outputLimit: Int = 64 * 1024
    ) async throws -> SubprocessResult {
        do {
            if let operation {
                return try await operation(executable, arguments, workingDirectory, outputLimit)
            }

            let result = try await Subprocess.run(
                .path(FilePath(executable)),
                arguments: Arguments(arguments),
                workingDirectory: workingDirectory.map { FilePath($0) },
                input: .none,
                output: .sequence,
                error: .combinedWithOutput
            ) { execution in
                var outputBytes: [UInt8] = []
                outputBytes.reserveCapacity(max(0, outputLimit))
                var outputWasTruncated = false

                for try await buffer in execution.standardOutput {
                    guard outputBytes.count < outputLimit else {
                        outputWasTruncated = true
                        continue
                    }
                    buffer.withUnsafeBytes { bytes in
                        let retainedByteCount = min(outputLimit - outputBytes.count, bytes.count)
                        outputBytes.append(contentsOf: bytes.prefix(retainedByteCount))
                        outputWasTruncated = outputWasTruncated || retainedByteCount < bytes.count
                    }
                }
                return (
                    output: String(decoding: outputBytes, as: UTF8.self),
                    wasTruncated: outputWasTruncated
                )
            }

            return SubprocessResult(
                succeeded: result.terminationStatus.isSuccess,
                output: result.closureResult.output,
                outputWasTruncated: result.closureResult.wasTruncated
            )
        } catch is CancellationError {
            throw CancellationError()
        } catch {
            throw CLIError.subprocessFailed(
                "could not execute \(executable): \(String(describing: error))"
            )
        }
    }
}
