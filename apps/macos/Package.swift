// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "UnkxwynoMirrorMacOS",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "UnkxwynoMirrorMacOS", targets: ["UnkxwynoMirrorMacOS"]),
    ],
    targets: [
        .target(name: "UnkxwynoMirrorMacOS"),
        .testTarget(name: "UnkxwynoMirrorMacOSTests", dependencies: ["UnkxwynoMirrorMacOS"]),
    ]
)
