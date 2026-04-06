// ManualControl.swift
import SwiftUI

struct ManualControlView: View {
    @EnvironmentObject var ble: BLEManager

    var body: some View {
        NavigationView {
            VStack(spacing: 32) {

                // ── Disconnected warning ──────────────────────────────────
                if ble.bleState != .connected {
                    HStack(spacing: 8) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundColor(.orange)
                        Text("Not connected to WateringSystem")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    .padding()
                    .background(Color.orange.opacity(0.1))
                    .cornerRadius(10)
                    .padding(.horizontal)
                }

                // ── Rain inhibit warning ──────────────────────────────────
                if ble.isRaining {
                    HStack(spacing: 8) {
                        Image(systemName: "cloud.rain.fill")
                            .foregroundColor(.blue)
                        Text("Rain detected — valves can still be opened manually")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    .padding()
                    .background(Color.blue.opacity(0.1))
                    .cornerRadius(10)
                    .padding(.horizontal)
                }

                // ── Valve controls ────────────────────────────────────────
                VStack(spacing: 20) {
                    ValveControl(
                        zone:     1,
                        isOpen:   ble.valve1Open,
                        disabled: ble.bleState != .connected
                    )
                    ValveControl(
                        zone:     2,
                        isOpen:   ble.valve2Open,
                        disabled: ble.bleState != .connected
                    )
                }
                .padding(.horizontal)

                Spacer()
            }
            .padding(.top, 16)
            .navigationTitle("Manual Control")
        }
    }
}

// ── Valve control card ────────────────────────────────────────────────────────
struct ValveControl: View {
    @EnvironmentObject var ble: BLEManager

    let zone:     Int
    let isOpen:   Bool
    let disabled: Bool

    // Safety timer — tracks how long the valve has been open manually
    @State private var secondsOpen: Int = 0
    @State private var timer: Timer?

    var body: some View {
        VStack(spacing: 16) {
            HStack {
                // Zone icon and label
                VStack(alignment: .leading, spacing: 4) {
                    Text("Zone \(zone)")
                        .font(.title2)
                        .fontWeight(.semibold)
                    Text(isOpen ? "Open" : "Closed")
                        .font(.subheadline)
                        .foregroundColor(isOpen ? .green : .secondary)
                }

                Spacer()

                // Status indicator
                Circle()
                    .fill(isOpen ? Color.green : Color.gray.opacity(0.3))
                    .frame(width: 16, height: 16)
            }

            // Safety timer — only shown when valve is open
            if isOpen && secondsOpen > 0 {
                HStack(spacing: 6) {
                    Image(systemName: "clock.fill")
                        .font(.caption)
                        .foregroundColor(.orange)
                    Text("Open for \(formattedTime)")
                        .font(.caption)
                        .foregroundColor(.orange)
                    Spacer()
                }
            }

            // Open / Close button
            Button(action: toggleValve) {
                Text(isOpen ? "Close Zone \(zone)" : "Open Zone \(zone)")
                    .fontWeight(.semibold)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 14)
                    .background(isOpen ? Color.red : Color.green)
                    .foregroundColor(.white)
                    .cornerRadius(12)
            }
            .disabled(disabled)
            .opacity(disabled ? 0.5 : 1.0)
        }
        .padding(20)
        .background(Color(.systemGray6))
        .cornerRadius(16)
        .onChange(of: isOpen) { open in
            if open {
                startTimer()
            } else {
                stopTimer()
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private func toggleValve() {
        let cmd = isOpen ? "V\(zone):0" : "V\(zone):1"
        ble.sendManualCommand(cmd)
    }

    private func startTimer() {
        secondsOpen = 0
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            secondsOpen += 1
        }
    }

    private func stopTimer() {
        timer?.invalidate()
        timer = nil
        secondsOpen = 0
    }

    private var formattedTime: String {
        let m = secondsOpen / 60
        let s = secondsOpen % 60
        if m > 0 {
            return "\(m)m \(s)s"
        } else {
            return "\(s)s"
        }
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────
struct ManualControlView_Previews: PreviewProvider {
    static var previews: some View {
        ManualControlView()
            .environmentObject(BLEManager())
    }
}
