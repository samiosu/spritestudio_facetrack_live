import Foundation

struct Pairing {
    let host: String
    let port: UInt16
    let token: String

    enum ValidationError: LocalizedError {
        case invalidURI, invalidHost, invalidPort, invalidToken
        var errorDescription: String? {
            switch self {
            case .invalidURI: return "spriteface://pair?v=2&host=...&port=...&token=... 形式の接続URIを入力してください。"
            case .invalidHost: return "PCのIPv4アドレスを入力してください。0.0.0.0・マルチキャスト・ブロードキャストは使えません。"
            case .invalidPort: return "ポート番号は1〜65535で入力してください。"
            case .invalidToken: return "PCが表示した32桁の16進トークンを入力してください。"
            }
        }
    }

    init(host: String, port: String, token: String) throws {
        let host = host.trimmingCharacters(in: .whitespacesAndNewlines)
        let octets = host.split(separator: ".", omittingEmptySubsequences: false)
        guard octets.count == 4,
              octets.allSatisfy({ !$0.isEmpty && $0.allSatisfy({ $0 >= "0" && $0 <= "9" }) && ($0.count == 1 || !$0.hasPrefix("0")) }),
              octets.allSatisfy({ UInt8($0) != nil }),
              let first = UInt8(octets[0]), first > 0, first < 224,
              host != "255.255.255.255" else { throw ValidationError.invalidHost }
        let portText = port.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !portText.isEmpty, portText.allSatisfy({ $0 >= "0" && $0 <= "9" }),
              let portValue = UInt16(portText), portValue > 0 else { throw ValidationError.invalidPort }
        let token = token.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard token.utf8.count == 32,
              token.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) }) else {
            throw ValidationError.invalidToken
        }
        self.host = host
        self.port = portValue
        self.token = token
    }

    init(uri: String) throws {
        guard let components = URLComponents(string: uri.trimmingCharacters(in: .whitespacesAndNewlines)),
              components.scheme == "spriteface", components.host == "pair",
              components.path.isEmpty, components.user == nil, components.password == nil,
              components.port == nil, components.fragment == nil,
              let items = components.queryItems, items.count == 4,
              Set(items.map(\.name)) == Set(["v", "host", "port", "token"]),
              let version = items.first(where: { $0.name == "v" })?.value, version == "2",
              let host = items.first(where: { $0.name == "host" })?.value,
              let port = items.first(where: { $0.name == "port" })?.value,
              let token = items.first(where: { $0.name == "token" })?.value else {
            throw ValidationError.invalidURI
        }
        try self.init(host: host, port: port, token: token)
    }
}
