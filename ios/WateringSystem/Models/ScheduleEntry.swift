import Foundation

// ── Day of week bitmask — mirrors scheduler.h on the ESP32 ───────────────────
struct DayMask: OptionSet, Codable {
    let rawValue: UInt8

    static let mon = DayMask(rawValue: 0x01)
    static let tue = DayMask(rawValue: 0x02)
    static let wed = DayMask(rawValue: 0x04)
    static let thu = DayMask(rawValue: 0x08)
    static let fri = DayMask(rawValue: 0x10)
    static let sat = DayMask(rawValue: 0x20)
    static let sun = DayMask(rawValue: 0x40)

    static let weekdays: DayMask = [.mon, .tue, .wed, .thu, .fri]
    static let weekend:  DayMask = [.sat, .sun]
    static let all:      DayMask = [.mon, .tue, .wed, .thu, .fri, .sat, .sun]

    // All days in display order
    static let ordered: [(DayMask, String)] = [
        (.mon, "Mon"), (.tue, "Tue"), (.wed, "Wed"),
        (.thu, "Thu"), (.fri, "Fri"), (.sat, "Sat"), (.sun, "Sun")
    ]

    // Converts to BLE wire format: "Mon+Wed+Fri"
    var bleString: String {
        DayMask.ordered
            .filter { self.contains($0.0) }
            .map    { $0.1 }
            .joined(separator: "+")
    }

    // Human-readable short label for UI
    var displayString: String {
        if self == .all      { return "Every day" }
        if self == .weekdays { return "Weekdays" }
        if self == .weekend  { return "Weekends" }
        return bleString.replacingOccurrences(of: "+", with: " ")
    }
}

// ── A single schedule entry ───────────────────────────────────────────────────
struct ScheduleEntry: Identifiable, Codable {
    var id       = UUID()
    var valve:       Int      // 1 or 2
    var dayMask:     DayMask
    var hour:        Int      // 0–23
    var minute:      Int      // 0–59
    var durationMin: Int      // 1–120
    var enabled:     Bool = true

    // Formatted time string for display: "07:00"
    var timeString: String {
        String(format: "%02d:%02d", hour, minute)
    }

    // Formatted duration for display: "20 min"
    var durationString: String {
        "\(durationMin) min"
    }
}
