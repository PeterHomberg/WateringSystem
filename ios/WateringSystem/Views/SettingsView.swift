// SettingsView.swift
import SwiftUI
import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var ble: BLEManager

    var body: some View {
        NavigationView {
            Form {

                // ── Connection ────────────────────────────────────────────
                Section(header: Text("Connection")) {
                    HStack {
                        Text("Status")
                        Spacer()
                        Text(connectionLabel)
                            .foregroundColor(connectionColor)
                    }

                    HStack {
                        Text("Device")
                        Spacer()
                        Text("WateringSystem")
                            .foregroundColor(.secondary)
                    }

                    if ble.bleState == .connected {
                        Button(role: .destructive, action: {
                            ble.disconnect()
                        }) {
                            HStack {
                                Image(systemName: "bluetooth.slash")
                                Text("Disconnect")
                            }
                        }
                    } else {
                        Button(action: {
                            ble.startScanning()
                        }) {
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                Text(ble.bleState == .scanning ? "Scanning…" : "Scan for Device")
                            }
                        }
                        .disabled(ble.bleState == .scanning || ble.bleState == .bluetoothOff)
                    }
                }

                // ── Live status ───────────────────────────────────────────
                Section(header: Text("Live Status")) {
                    HStack {
                        Text("Rain sensor")
                        Spacer()
                        Text(ble.isRaining ? "Raining" : "Dry")
                            .foregroundColor(ble.isRaining ? .blue : .secondary)
                    }
                    HStack {
                        Text("Zone 1")
                        Spacer()
                        Text(ble.valve1Open ? "Open" : "Closed")
                            .foregroundColor(ble.valve1Open ? .green : .secondary)
                    }
                    HStack {
                        Text("Zone 2")
                        Spacer()
                        Text(ble.valve2Open ? "Open" : "Closed")
                            .foregroundColor(ble.valve2Open ? .green : .secondary)
                    }
                    HStack {
                        Text("Last schedule sync")
                        Spacer()
                        Text(ble.scheduleAck.isEmpty ? "—" : ble.scheduleAck)
                            .foregroundColor(
                                ble.scheduleAck == "SCH:OK" ? .green :
                                ble.scheduleAck == "SCH:ERR" ? .red : .secondary)
                    }
                }

                // ── About ─────────────────────────────────────────────────
                Section(header: Text("About")) {
                    HStack {
                        Text("App version")
                        Spacer()
                        Text(appVersion)
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Protocol")
                        Spacer()
                        Text("Bluetooth LE")
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Controller")
                        Spacer()
                        Text("ESP32-C3")
                            .foregroundColor(.secondary)
                    }
                }

                // ── Help ──────────────────────────────────────────────────
                Section(header: Text("Help")) {
                    VStack(alignment: .leading, spacing: 8) {
                        Label("Make sure Bluetooth is enabled on your iPhone",
                              systemImage: "bluetooth")
                        Label("Stay within range of the controller (~10m)",
                              systemImage: "antenna.radiowaves.left.and.right")
                        Label("The app reconnects automatically when in range",
                              systemImage: "arrow.clockwise")
                    }
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                    .padding(.vertical, 4)
                }
            }
            .navigationTitle("Settings")
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private var connectionLabel: String {
        switch ble.bleState {
        case .bluetoothOff:  return "Bluetooth Off"
        case .scanning:      return "Scanning…"
        case .connecting:    return "Connecting…"
        case .connected:     return "Connected"
        case .disconnected:  return "Disconnected"
        }
    }

    private var connectionColor: Color {
        switch ble.bleState {
        case .connected:              return .green
        case .scanning, .connecting:  return .orange
        default:                      return .red
        }
    }

    private var appVersion: String {
        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
        let build   = Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
        return "\(version) (\(build))"
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────
struct SettingsView_Previews: PreviewProvider {
    static var previews: some View {
        SettingsView()
            .environmentObject(BLEManager())
    }
}
