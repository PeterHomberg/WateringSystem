import SwiftUI

struct DashboardView: View {
    @EnvironmentObject var ble: BLEManager

    var body: some View {
        NavigationView {
            VStack(spacing: 24) {

                // ── Connection status banner ──────────────────────────────
                ConnectionBanner(state: ble.bleState)

                // ── Rain sensor card — full width ─────────────────────────
                RainSensorCard(isRaining: ble.isRaining, rainLevel: ble.rainLevel)
                    .padding(.horizontal)

                // ── Status cards ──────────────────────────────────────────
                VStack(spacing: 16) {
                    HStack(spacing: 16) {
                        StatusCard(
                            title: "Zone 1",
                            value: ble.valve1Open ? "Open" : "Closed",
                            icon:  ble.valve1Open ? "drop.fill" : "drop",
                            color: ble.valve1Open ? .green : .secondary
                        )
                        StatusCard(
                            title: "Zone 2",
                            value: ble.valve2Open ? "Open" : "Closed",
                            icon:  ble.valve2Open ? "drop.fill" : "drop",
                            color: ble.valve2Open ? .green : .secondary
                        )
                    }
                    HStack(spacing: 16) {
                        StatusCard(
                            title: "Schedule",
                            value: ble.scheduleAck == "SCH:OK" ? "Saved" :
                                   ble.scheduleAck == "SCH:ERR" ? "Error" : "—",
                            icon:  ble.scheduleAck == "SCH:OK" ? "checkmark.circle.fill" :
                                   ble.scheduleAck == "SCH:ERR" ? "xmark.circle.fill" :
                                   "calendar",
                            color: ble.scheduleAck == "SCH:OK" ? .green :
                                   ble.scheduleAck == "SCH:ERR" ? .red : .secondary
                        )
                        StatusCard(
                            title: "Time",
                            value: ble.currentTime.isEmpty ? "—" : ble.currentTime,
                            icon:  "clock",
                            color: .secondary
                        )
                    }
                }
                .padding(.horizontal)

                // ── Rain inhibit notice ───────────────────────────────────
                if ble.isRaining {
                    HStack(spacing: 8) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundColor(.orange)
                        Text("Watering inhibited — rain detected")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    .padding()
                    .background(Color.orange.opacity(0.1))
                    .cornerRadius(10)
                    .padding(.horizontal)
                }

                Spacer()
            }
            .navigationTitle("Watering System")
        }
    }
}

// ── Rain sensor card ──────────────────────────────────────────────────────────
struct RainSensorCard: View {
    let isRaining: Bool
    let rainLevel: Int   // 0–100

    // Colour transitions dry (orange) → damp (yellow) → wet (blue)
    private var levelColor: Color {
        if rainLevel < 20  { return .orange }
        if rainLevel < 50  { return .yellow }
        return .blue
    }

    private var levelLabel: String {
        switch rainLevel {
        case 0:       return "Dry"
        case 1..<20:  return "Trace"
        case 20..<50: return "Damp"
        case 50..<80: return "Wet"
        default:      return "Soaking"
        }
    }

    var body: some View {
        VStack(spacing: 12) {
            // Header row
            HStack {
                Image(systemName: isRaining ? "cloud.rain.fill" : "sun.max.fill")
                    .font(.system(size: 28))
                    .foregroundColor(isRaining ? .blue : .orange)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Rain Sensor")
                        .font(.headline)
                    Text(isRaining ? "Rain detected" : "No rain")
                        .font(.subheadline)
                        .foregroundColor(isRaining ? .blue : .secondary)
                }
                Spacer()
                // Digital status badge
                Text(isRaining ? "RAIN" : "DRY")
                    .font(.caption)
                    .fontWeight(.bold)
                    .foregroundColor(.white)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 5)
                    .background(isRaining ? Color.blue : Color.orange)
                    .cornerRadius(8)
            }

            // Analog level bar
            VStack(spacing: 6) {
                HStack {
                    Text("Wetness")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Spacer()
                    Text("\(rainLevel)%  \(levelLabel)")
                        .font(.caption)
                        .fontWeight(.medium)
                        .foregroundColor(levelColor)
                }

                // Progress bar
                GeometryReader { geo in
                    ZStack(alignment: .leading) {
                        // Track
                        RoundedRectangle(cornerRadius: 6)
                            .fill(Color(.systemGray5))
                            .frame(height: 12)
                        // Fill
                        RoundedRectangle(cornerRadius: 6)
                            .fill(levelColor)
                            .frame(width: geo.size.width * CGFloat(rainLevel) / 100.0,
                                   height: 12)
                            .animation(.easeInOut(duration: 0.4), value: rainLevel)
                    }
                }
                .frame(height: 12)

                // Scale labels
                HStack {
                    Text("0%")
                    Spacer()
                    Text("50%")
                    Spacer()
                    Text("100%")
                }
                .font(.system(size: 10))
                .foregroundColor(.secondary)
            }
        }
        .padding(16)
        .background(Color(.systemGray6))
        .cornerRadius(16)
    }
}

// ── Connection banner ─────────────────────────────────────────────────────────
struct ConnectionBanner: View {
    let state: BLEState

    var label: String {
        switch state {
        case .bluetoothOff:  return "Bluetooth Off"
        case .scanning:      return "Scanning…"
        case .connecting:    return "Connecting…"
        case .connected:     return "Connected"
        case .disconnected:  return "Disconnected"
        }
    }

    var color: Color {
        switch state {
        case .connected:            return .green
        case .scanning, .connecting: return .orange
        case .bluetoothOff,
             .disconnected:         return .red
        }
    }

    var icon: String {
        switch state {
        case .connected:             return "bluetooth"
        case .scanning, .connecting: return "antenna.radiowaves.left.and.right"
        default:                     return "bluetooth.slash"
        }
    }

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: icon)
            Text(label)
                .fontWeight(.medium)
        }
        .foregroundColor(.white)
        .padding(.horizontal, 20)
        .padding(.vertical, 10)
        .background(color)
        .cornerRadius(20)
        .padding(.top, 8)
    }
}

// ── Status card ───────────────────────────────────────────────────────────────
struct StatusCard: View {
    let title: String
    let value: String
    let icon:  String
    let color: Color

    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: icon)
                .font(.system(size: 32))
                .foregroundColor(color)
            Text(value)
                .font(.headline)
                .foregroundColor(.primary)
            Text(title)
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 20)
        .background(Color(.systemGray6))
        .cornerRadius(14)
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────
struct DashboardView_Previews: PreviewProvider {
    static var previews: some View {
        DashboardView()
            .environmentObject(BLEManager())
    }
}
