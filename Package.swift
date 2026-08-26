// swift-tools-version: 6.3

import PackageDescription

let package = Package(
    name: "playdate-cli",
    platforms: [
        .macOS(.v15)
    ],
    products: [
        .executable(
            name: "playdate-simctl",
            targets: ["PlaydateSimulatorCLI"]
        ),
        .library(
            name: "PlaydateSimulatorAgent",
            type: .dynamic,
            targets: ["PlaydateSimulatorAgent"]
        ),
    ],
    dependencies: [
        .package(
            url: "https://github.com/apple/swift-argument-parser.git",
            exact: "1.8.2"
        ),
        .package(
            url: "https://github.com/swiftlang/swift-subprocess.git",
            exact: "1.0.0"
        ),
    ],
    targets: [
        .executableTarget(
            name: "PlaydateSimulatorCLI",
            dependencies: [
                "SocketSupport",
                .product(name: "ArgumentParser", package: "swift-argument-parser"),
                .product(name: "Subprocess", package: "swift-subprocess"),
            ]
        ),
        .target(
            name: "PlaydateSimulatorAgent",
            dependencies: [
                "AgentCore",
                "CaptureSupport",
                "DescriptorIO",
                "GIFEncoder",
                "SocketSupport",
            ],
            linkerSettings: [
                .linkedFramework("CoreGraphics"),
                .linkedFramework("ImageIO"),
            ]
        ),
        .target(
            name: "AgentCore",
            dependencies: ["SocketSupport"],
            publicHeadersPath: "include"
        ),
        .target(
            name: "CaptureSupport",
            dependencies: ["DescriptorIO"],
            publicHeadersPath: "include"
        ),
        .target(
            name: "DescriptorIO",
            publicHeadersPath: "include"
        ),
        .target(
            name: "GIFEncoder",
            publicHeadersPath: "include"
        ),
        .target(
            name: "SocketSupport",
            dependencies: ["DescriptorIO"],
            publicHeadersPath: "include"
        ),
        .target(
            name: "SocketTestSupport",
            dependencies: ["DescriptorIO", "SocketSupport"],
            path: "Tests/SocketTestSupport",
            publicHeadersPath: "include"
        ),
        .testTarget(
            name: "PlaydateSimulatorCLITests",
            dependencies: [
                "AgentCore",
                "CaptureSupport",
                "DescriptorIO",
                "GIFEncoder",
                "PlaydateSimulatorCLI",
                "SocketSupport",
                "SocketTestSupport",
                .product(name: "ArgumentParser", package: "swift-argument-parser"),
            ]
        ),
    ]
)
