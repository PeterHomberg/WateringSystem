import Foundation
import CoreBluetooth

// ── BLE UUIDs — must match config.h on the ESP32 ─────────────────────────────
private let SERVICE_UUID   = CBUUID(string: "12345678-1234-1234-1234-123456789000")
private let STATUS_UUID    = CBUUID(string: "12345678-1234-1234-1234-123456789001")
private let SCHEDULE_UUID  = CBUUID(string: "12345678-1234-1234-1234-123456789002")
private let RAIN_UUID      = CBUUID(string: "12345678-1234-1234-1234-123456789003")
private let MANUAL_UUID    = CBUUID(string: "12345678-1234-1234-1234-123456789004")
private let TIME_UUID      = CBUUID(string: "12345678-1234-1234-1234-123456789005")

// ── Connection state ──────────────────────────────────────────────────────────
enum BLEState {
    case bluetoothOff
    case scanning
    case connecting
    case connected
    case disconnected
}

// ── Main BLE manager ──────────────────────────────────────────────────────────
class BLEManager: NSObject, ObservableObject {

    // Published properties — SwiftUI views update automatically when these change
    @Published var bleState: BLEState  = .disconnected
    @Published var isRaining: Bool     = false
    @Published var rainLevel: Int      = 0        // 0–100 from analog sensor
    @Published var valve1Open: Bool    = false
    @Published var valve2Open: Bool    = false
    @Published var scheduleAck: String = ""       // "SCH:OK" or "SCH:ERR"
    @Published var currentTime: String = ""       // "HH:MM" from RTC
    @Published var timeSynced: Bool    = false    // true after successful time sync
    @Published var lastError: String   = ""

    // Internal CoreBluetooth objects
    private var centralManager:  CBCentralManager!
    private var peripheral:      CBPeripheral?
    private var statusChar:      CBCharacteristic?
    private var scheduleChar:    CBCharacteristic?
    private var rainChar:        CBCharacteristic?
    private var manualChar:      CBCharacteristic?
    private var timeChar:        CBCharacteristic?

    // Auto-reconnect
    private var autoReconnect = true

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // ── Public controls ───────────────────────────────────────────────────────

    func startScanning() {
        guard centralManager.state == .poweredOn else { return }
        bleState = .scanning
        centralManager.scanForPeripherals(withServices: [SERVICE_UUID])
        print("BLE: scanning started")
    }

    func stopScanning() {
        centralManager.stopScan()
        print("BLE: scanning stopped")
    }

    func disconnect() {
        autoReconnect = false
        if let p = peripheral {
            centralManager.cancelPeripheralConnection(p)
        }
    }

    // Send a manual valve command: "V1:1", "V1:0", "V2:1", "V2:0"
    func sendManualCommand(_ cmd: String) {
        write(cmd, to: manualChar)
    }

    // Send full schedule — takes an array of ScheduleEntry and writes them
    // one at a time following the SCHED:BEGIN / SCHED:END protocol
    func sendSchedule(_ entries: [ScheduleEntry]) {
        var lines: [String] = ["SCHED:BEGIN"]
        for entry in entries {
            let days = entry.dayMask.bleString
            let time = String(format: "%02d:%02d", entry.hour, entry.minute)
            let zone = "V\(entry.valve)"
            lines.append("SCHED:\(zone),\(days),\(time),\(entry.durationMin)")
        }
        lines.append("SCHED:END")

        for (index, line) in lines.enumerated() {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(index) * 0.15) {
                self.write(line, to: self.scheduleChar)
                print("BLE schedule tx: \(line)")
            }
        }
    }

    // Manually trigger a time sync — exposed for the Settings "Sync Time" button
    func syncTime() {
        sendCurrentTime()
    }

    // ── Internal helpers ──────────────────────────────────────────────────────

    private func write(_ value: String, to characteristic: CBCharacteristic?) {
        guard let p = peripheral,
              let char = characteristic,
              let data = value.data(using: .utf8) else {
            print("BLE write failed — not connected or characteristic missing")
            return
        }
        p.writeValue(data, for: char, type: .withResponse)
    }

    // Build and send TIME string from iPhone's current clock
    // Format: "TIME:2026-05-03,14:32:00,6"
    //   date:    YYYY-MM-DD
    //   time:    HH:MM:SS
    //   weekday: 0=Mon … 6=Sun (matches ESP32 scheduler bitmask convention)
    private func sendCurrentTime() {
        let now = Date()
        let cal = Calendar.current

        let year    = cal.component(.year,   from: now)
        let month   = cal.component(.month,  from: now)
        let day     = cal.component(.day,    from: now)
        let hour    = cal.component(.hour,   from: now)
        let minute  = cal.component(.minute, from: now)
        let second  = cal.component(.second, from: now)

        // Calendar.weekday: 1=Sun … 7=Sat — convert to 0=Mon … 6=Sun
        let rawWeekday = cal.component(.weekday, from: now)  // 1=Sun
        let weekday = (rawWeekday + 5) % 7                   // 0=Mon … 6=Sun

    let timeString = String(format: "TIME:%04d-%02d-%02dT%02d:%02d:%02dW%d",
                            year, month, day, hour, minute, second, weekday)
        print("BLE time sync: \(timeString)")
        write(timeString, to: timeChar)
        timeSynced = true
    }

    // Parse STATUS string: "R:0,L:42,V1:0,V2:0,SCH:OK,T:14:30"
    // Also handles old format without L and T fields for backwards compatibility
    private func parseStatus(_ value: String) {
        let parts = value.split(separator: ",")
        for part in parts {
            let kv = part.split(separator: ":", maxSplits: 1)
            guard kv.count == 2 else { continue }
            let key = String(kv[0])
            let val = String(kv[1])
            switch key {
            case "R":   isRaining   = val == "1"
            case "L":   rainLevel   = Int(val) ?? 0
            case "V1":  valve1Open  = val == "1"
            case "V2":  valve2Open  = val == "1"
            case "SCH": scheduleAck = "SCH:\(val)"
            case "T":   currentTime = val
            default:    break
            }
        }
    }

    // Parse RAIN characteristic string: "R:0,L:42"
    private func parseRain(_ value: String) {
        let parts = value.split(separator: ",")
        for part in parts {
            let kv = part.split(separator: ":", maxSplits: 1)
            guard kv.count == 2 else { continue }
            let key = String(kv[0])
            let val = String(kv[1])
            switch key {
            case "R": isRaining = val == "1"
            case "L": rainLevel = Int(val) ?? 0
            default:  break
            }
        }
    }
}

// ── CBCentralManagerDelegate ──────────────────────────────────────────────────
extension BLEManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("BLE: powered on — starting scan")
            bleState = .scanning
            startScanning()
        case .poweredOff:
            bleState = .bluetoothOff
            lastError = "Bluetooth is turned off"
        default:
            bleState = .disconnected
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        print("BLE: found \(peripheral.name ?? "unknown")")
        self.peripheral = peripheral
        centralManager.stopScan()
        bleState = .connecting
        centralManager.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        print("BLE: connected to \(peripheral.name ?? "device")")
        bleState = .connected
        peripheral.delegate = self
        peripheral.discoverServices([SERVICE_UUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        print("BLE: disconnected")
        bleState     = .disconnected
        timeSynced   = false
        statusChar   = nil
        scheduleChar = nil
        rainChar     = nil
        manualChar   = nil
        timeChar     = nil

        if autoReconnect {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.startScanning()
            }
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        print("BLE: failed to connect — \(error?.localizedDescription ?? "")")
        bleState = .disconnected
        if autoReconnect { startScanning() }
    }
}

// ── CBPeripheralDelegate ──────────────────────────────────────────────────────
extension BLEManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            if service.uuid == SERVICE_UUID {
                peripheral.discoverCharacteristics(
                    [STATUS_UUID, SCHEDULE_UUID, RAIN_UUID, MANUAL_UUID, TIME_UUID],
                    for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            switch char.uuid {
            case STATUS_UUID:
                statusChar = char
                peripheral.setNotifyValue(true, for: char)
                peripheral.readValue(for: char)
            case SCHEDULE_UUID:
                scheduleChar = char
            case RAIN_UUID:
                rainChar = char
                peripheral.setNotifyValue(true, for: char)
                peripheral.readValue(for: char)
            case MANUAL_UUID:
                manualChar = char
            case TIME_UUID:
                timeChar = char
                // Automatically sync time as soon as characteristic is discovered
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                    self.sendCurrentTime()
                }
            default:
                break
            }
        }
        print("BLE: all characteristics discovered and ready")
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard error == nil,
              let data = characteristic.value,
              let str  = String(data: data, encoding: .utf8) else { return }

        DispatchQueue.main.async {
            switch characteristic.uuid {
            case STATUS_UUID:
                print("BLE status: \(str)")
                self.parseStatus(str)
            case RAIN_UUID:
                print("BLE rain: \(str)")
                self.parseRain(str)
            default:
                break
            }
        }
    }
}
