// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "CodexDeckCompanion",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "CodexDeckCompanion", targets: ["CodexDeckCompanion"]),
    ],
    targets: [
        .executableTarget(name: "CodexDeckCompanion"),
        .testTarget(
            name: "CodexDeckCompanionTests",
            dependencies: ["CodexDeckCompanion"]
        ),
    ],
    swiftLanguageModes: [.v5]
)
