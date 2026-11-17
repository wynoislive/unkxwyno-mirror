// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "UnkxwynoMirrorIOS",
    platforms: [.iOS(.v17)],
    products: [
        .library(name: "UnkxwynoMirrorIOS", targets: ["UnkxwynoMirrorIOS"]),
    ],
    targets: [
        .target(name: "UnkxwynoMirrorIOS"),
        .testTarget(name: "UnkxwynoMirrorIOSTests", dependencies: ["UnkxwynoMirrorIOS"]),
    ]
)
