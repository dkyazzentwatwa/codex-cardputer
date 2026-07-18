import Foundation

enum TerminalText {
    private static let ansiPattern = "\u{001B}\\[[0-?]*[ -/]*[@-~]"
    private static let urlPattern = "https://[^\\s]+"
    private static let deviceCodePattern = "\\b[A-Z0-9]{4}-[A-Z0-9]{5}\\b"

    static func clean(_ value: String) -> String {
        value.replacingOccurrences(of: ansiPattern, with: "", options: .regularExpression)
    }

    static func firstURL(in value: String) -> URL? {
        guard let range = value.range(of: urlPattern, options: .regularExpression) else { return nil }
        return URL(string: String(value[range]).trimmingCharacters(in: CharacterSet(charactersIn: ".,")))
    }

    static func deviceCode(in value: String) -> String? {
        value.range(of: deviceCodePattern, options: .regularExpression).map { String(value[$0]) }
    }
}
