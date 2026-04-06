// DashboardView.swift
import SwiftUI
import SwiftUI

struct DashboardView: View {
    @EnvironmentObject var ble: BLEManager

    var body: some View {
        NavigationView {
            VStack(spacing: 24) {

                // ── Connection status banner ──────────────────────────────
                ConnectionBanner(state: ble.bleState)

                // ── Status cards ──────────────────────────────────────────
                VStack(spacing: 16) {
                    HStack(spacing: 16) {
                        StatusCard(
                            title: "Rain",
                            value: ble.isRaining ? "Raining" : "Dry",
                            icon:  ble.isRaining ? "cloud.rain.fill" : "sun.max.fill",
                            color: ble.isRaining ? .blue : .orange
                        )
                        StatusCard(
                            title: "Zone 1",
                            value: ble.valve1Open ? "Open" : "Closed",
                            icon:  ble.valve1Open ? "drop.fill" : "drop",
                            color: ble.valve1Open ? .green : .secondary
                        )
                    }
                    HStack(spacing: 16) {
                        StatusCard(
                            title: "Zone 2",
                            value: ble.valve2Open ? "Open" : "Closed",
                            icon:  ble.valve2Open ? "drop.fill" : "drop",
                            color: ble.valve2Open ? .green : .secondary
                        )
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
        case .connected:     return .green
        case .scanning,
             .connecting:    return .orange
        case .bluetoothOff,
             .disconnected:  return .red
        }
    }

    var icon: String {
        switch state {
        case .connected:    return "bluetooth"
        case .scanning,
             .connecting:   return "antenna.radiowaves.left.and.right"
        default:            return "bluetooth.slash"
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
