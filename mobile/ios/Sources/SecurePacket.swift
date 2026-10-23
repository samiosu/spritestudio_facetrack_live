import CryptoKit
import Foundation
import Security

enum SecurePacket {
    private static let magic = Data("SF2".utf8)

    static func challenge() -> String {
        var bytes = [UInt8](repeating: 0, count: 16)
        precondition(SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess)
        return bytes.map { String(format: "%02x", $0) }.joined()
    }

    static func validChallenge(_ value: String) -> Bool {
        value.utf8.count == 32 && value.utf8.allSatisfy {
            (48...57).contains($0) || (97...102).contains($0)
        }
    }

    private static func key(_ token: String) -> SymmetricKey {
        precondition(validChallenge(token))
        let bytes = stride(from: 0, to: 32, by: 2).map { offset in
            UInt8(String(token.dropFirst(offset).prefix(2)), radix: 16)!
        }
        return SymmetricKey(data: Data(bytes))
    }

    static func seal(_ plaintext: Data, token: String) throws -> Data {
        let sealed = try AES.GCM.seal(plaintext, using: key(token), authenticating: magic)
        return magic + sealed.combined!
    }

    static func open(_ packet: Data, token: String) throws -> Data {
        guard packet.count >= magic.count + 12 + 16 && packet.starts(with: magic) else {
            throw CocoaError(.coderInvalidValue)
        }
        let sealed = try AES.GCM.SealedBox(combined: packet.dropFirst(magic.count))
        return try AES.GCM.open(sealed, using: key(token), authenticating: magic)
    }
}
