import Foundation
import SocketSupport

struct AgentClient: Sendable {
    typealias SendOperation =
        @Sendable (_ request: AgentRequest, _ processIdentifier: Int32) async throws -> String

    private static let defaultConnectTimeoutMilliseconds = UInt32(
        PDSIM_SOCKET_IO_TIMEOUT_MILLISECONDS
    )
    private static let defaultResponseTimeoutMilliseconds: UInt32 = 15_000

    private let socketPathProvider: @Sendable (Int32) -> String
    private let connectTimeoutMilliseconds: UInt32
    private let responseTimeoutMilliseconds: UInt32
    private let sendOperation: SendOperation?

    init(
        socketPathProvider: @escaping @Sendable (Int32) -> String = {
            AgentSocketPath.path(processIdentifier: $0)
        },
        connectTimeoutMilliseconds: UInt32 = Self.defaultConnectTimeoutMilliseconds,
        responseTimeoutMilliseconds: UInt32 = Self.defaultResponseTimeoutMilliseconds
    ) {
        self.socketPathProvider = socketPathProvider
        self.connectTimeoutMilliseconds = connectTimeoutMilliseconds
        self.responseTimeoutMilliseconds = responseTimeoutMilliseconds
        self.sendOperation = nil
    }

    init(sendOperation: @escaping SendOperation) {
        self.socketPathProvider = { _ in "" }
        self.connectTimeoutMilliseconds = Self.defaultConnectTimeoutMilliseconds
        self.responseTimeoutMilliseconds = Self.defaultResponseTimeoutMilliseconds
        self.sendOperation = sendOperation
    }

    @concurrent
    func send(_ request: AgentRequest, processIdentifier: Int32) async throws -> String {
        if let sendOperation {
            return try await sendOperation(request, processIdentifier)
        }

        let socketPath = socketPathProvider(processIdentifier)
        let requestLine = request.line
        try AgentProtocol.validate(line: requestLine)

        var response = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
        var errorMessage = [CChar](repeating: 0, count: AgentProtocol.bufferCapacity)
        let configuration = pdsim_socket_configuration(
            expected_peer_process_identifier: processIdentifier,
            connect_timeout_milliseconds: connectTimeoutMilliseconds,
            response_timeout_milliseconds: responseTimeoutMilliseconds
        )

        let result = socketPath.withCString { socketPathPointer in
            requestLine.withCString { requestPointer in
                unsafe pdsim_send_command(
                    socketPathPointer,
                    requestPointer,
                    configuration,
                    &response,
                    response.count,
                    &errorMessage,
                    errorMessage.count
                )
            }
        }

        let decodedError = Self.decodeLossyCString(errorMessage)
        switch result {
        case pdsim_socket_success:
            break
        case pdsim_socket_agent_not_running:
            throw CLIError.agentNotRunning(decodedError)
        case pdsim_socket_peer_mismatch:
            throw CLIError.agentUnavailable(decodedError)
        case pdsim_socket_protocol_error:
            throw CLIError.agentUnavailable(decodedError)
        case pdsim_socket_timeout, pdsim_socket_io_error, pdsim_socket_invalid_argument:
            throw CLIError.agentConnectionFailed(decodedError)
        default:
            throw CLIError.agentConnectionFailed("unknown socket transport error")
        }

        guard let responseString = Self.decodeCString(response) else {
            throw CLIError.agentUnavailable("the agent returned invalid UTF-8")
        }
        guard !responseString.hasPrefix("error ") else {
            throw CLIError.agentUnavailable(String(responseString.dropFirst("error ".count)))
        }
        guard responseString == "ok" || responseString.hasPrefix("ok ") else {
            throw CLIError.agentUnavailable("unexpected agent response: \(responseString)")
        }
        return responseString
    }

    private static func decodeCString(_ characters: [CChar]) -> String? {
        let bytes =
            characters
            .prefix { $0 != 0 }
            .map { UInt8(bitPattern: $0) }
        return String(bytes: bytes, encoding: .utf8)
    }

    private static func decodeLossyCString(_ characters: [CChar]) -> String {
        let bytes =
            characters
            .prefix { $0 != 0 }
            .map { UInt8(bitPattern: $0) }
        return String(decoding: bytes, as: UTF8.self)
    }
}
