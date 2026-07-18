import Darwin
import Foundation

enum NetworkAddresses {
    struct NamedAddress: Equatable {
        var name: String
        var address: String
    }

    static func privateIPv4() -> [NamedAddress] {
        var pointer: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&pointer) == 0, let first = pointer else { return [] }
        defer { freeifaddrs(first) }
        var results: [NamedAddress] = []
        var current: UnsafeMutablePointer<ifaddrs>? = first
        while let interface = current {
            defer { current = interface.pointee.ifa_next }
            guard let socketAddress = interface.pointee.ifa_addr,
                  socketAddress.pointee.sa_family == UInt8(AF_INET) else { continue }
            var address = socketAddress.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { $0.pointee.sin_addr }
            var buffer = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
            guard inet_ntop(AF_INET, &address, &buffer, socklen_t(INET_ADDRSTRLEN)) != nil else { continue }
            let value = String(cString: buffer)
            guard isPrivate(value) else { continue }
            results.append(NamedAddress(name: String(cString: interface.pointee.ifa_name), address: value))
        }
        return results.sorted { $0.name == $1.name ? $0.address < $1.address : $0.name < $1.name }
    }

    static func isPrivate(_ address: String) -> Bool {
        let parts = address.split(separator: ".").compactMap { Int($0) }
        guard parts.count == 4 else { return false }
        return parts[0] == 10
            || (parts[0] == 172 && (16...31).contains(parts[1]))
            || (parts[0] == 192 && parts[1] == 168)
    }
}
