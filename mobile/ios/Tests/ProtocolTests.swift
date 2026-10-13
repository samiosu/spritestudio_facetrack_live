import XCTest
@testable import SpriteFacePhone

final class ProtocolTests: XCTestCase {
    private let token = "0123456789abcdef0123456789abcdef"

    func testPairingValidatesEndpointAndCapability() throws {
        let pairing = try Pairing(uri: "spriteface://pair?v=1&host=192.168.1.2&port=11574&token=\(token)")
        XCTAssertEqual(pairing.host, "192.168.1.2")
        XCTAssertEqual(pairing.port, 11574)
        XCTAssertEqual(pairing.token, token)
        for host in ["0.0.0.0", "224.0.0.1", "255.255.255.255", "pc.local", "192.168.1.256", "192.168.01.2"] {
            XCTAssertThrowsError(try Pairing(host: host, port: "11574", token: token))
        }
        XCTAssertThrowsError(try Pairing(host: "192.168.1.2", port: "65536", token: token))
        XCTAssertThrowsError(try Pairing(host: "192.168.1.2", port: "11574", token: "wrong"))
        XCTAssertThrowsError(try Pairing(uri: "spriteface://pair?v=1&host=192.168.1.2&port=11574&token=\(token)&host=127.0.0.1"))
        XCTAssertThrowsError(try Pairing(uri: "spriteface://pair?v=2&host=192.168.1.2&port=11574&token=\(token)"))
    }

    func testNeutralFrameMatchesWireProtocol() throws {
        var frame = FaceFrame.neutral(token: token)
        frame.seq = UInt32.max
        let data = try JSONEncoder().encode(frame)
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        XCTAssertEqual(object["v"] as? Int, 1)
        XCTAssertEqual(object["type"] as? String, "frame")
        XCTAssertEqual((object["seq"] as? NSNumber)?.uint32Value, UInt32.max)
        XCTAssertEqual(object["tracking"] as? Bool, false)
        XCTAssertEqual((object["eyes"] as? [String: Double])?["left"], 1)
        XCTAssertEqual((object["mouth"] as? [String: Double])?["open"], 0)
        XCTAssertEqual(try JSONDecoder().decode(FaceFrame.self, from: data).token, token)
    }

    func testEulerAnglesIgnoreUniformScale() {
        let yaw = 0.3, pitch = -0.2, roll = 0.1
        let cy = cos(yaw), sy = sin(yaw), cx = cos(pitch), sx = sin(pitch), cz = cos(roll), sz = sin(roll)
        let matrix = [cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                      sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                      -sy, cy * sx, cy * cx].map { $0 * 2 }
        let head = FaceFrame.headFromRotation(matrix)
        XCTAssertEqual(head.yaw, yaw, accuracy: 0.000001)
        XCTAssertEqual(head.pitch, pitch, accuracy: 0.000001)
        XCTAssertEqual(head.roll, roll, accuracy: 0.000001)
    }
}
