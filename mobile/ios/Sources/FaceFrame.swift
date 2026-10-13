import Foundation

struct FaceFrame: Codable {
    struct Head: Codable {
        var yaw: Double = 0
        var pitch: Double = 0
        var roll: Double = 0
    }
    struct Eyes: Codable {
        var left: Double = 1
        var right: Double = 1
    }
    struct Mouth: Codable {
        var open: Double = 0
        var smile: Double = 0
    }

    var v = 1
    var type = "frame"
    var token: String
    var seq: UInt32 = 0
    var tracking = false
    var confidence: Double = 0
    var head = Head()
    var eyes = Eyes()
    var mouth = Mouth()

    static func neutral(token: String = "") -> FaceFrame { FaceFrame(token: token) }

    static func unit(_ value: Double) -> Double {
        value.isFinite ? min(1, max(0, value)) : 0
    }

    /// A scaled 3x3 row-major rotation is decomposed as Rz(roll) Ry(yaw) Rx(pitch).
    /// This is shared by the native phone adapters. Angles are radians.
    static func headFromRotation(_ matrix: [Double]) -> Head {
        guard matrix.count == 9, matrix.allSatisfy(\.isFinite) else { return Head() }
        var r = matrix
        for column in 0..<3 {
            let length = sqrt(r[column] * r[column] + r[3 + column] * r[3 + column] + r[6 + column] * r[6 + column])
            guard length > 0.000001 else { return Head() }
            for row in 0..<3 { r[row * 3 + column] /= length }
        }
        let yaw = asin(min(1, max(-1, -r[6])))
        // At gimbal lock choose roll = 0; yaw remains well-defined.
        if abs(cos(yaw)) < 0.000001 {
            return Head(yaw: yaw, pitch: atan2(-r[5], r[4]), roll: 0)
        }
        return Head(yaw: yaw, pitch: atan2(r[7], r[8]), roll: atan2(r[3], r[0]))
    }
}
