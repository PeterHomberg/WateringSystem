import Foundation
import CoreBluetooth

// ── BLE UUIDs — must match config.h on the ESP32 ─────────────────────────────
private let SERVICE_UUID   = CBUUID(string: "12345678-1234-1234-1234-123456789000")
private let STATUS_UUID    = CBUUID(string: "12345678-1234-1234-1234-123456789001")
private let SCHEDULE_UUID  = CBUUID(string: "12345678-1234-1234-1234-123456789002")
private let RAIN_UUID      = CBUUID(string: "12345678-1234-1234-1234-123456789003")
private let MANUAL_UUID    = CBUUID(string: "12345678-1234-1234-1234-123456789004")

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
    @Published var bleState: BLEState = .disconnected
    @Published var isRaining: Bool    = false
    @Published var valve1Open: Bool   = false
    @Published var valve2Open: Bool   = false
    @Published var scheduleAck: String = ""          // "SCH:OK" or "SCH:ERR"
    @Published var lastError: String  = ""

    // Internal CoreBluetooth objects
    private var centralManager:   CBCentralManager!
    private var peripheral:       CBPeripheral?
    private var statusChar:       CBCharacteristic?
    private var scheduleChar:     CBCharacteristic?
    private var rainChar:         CBCharacteristic?
    private var manualChar:       CBCharacteristic?

    // Auto-reconnect
    private var autoReconnect = true

    override init() {
        super.init()
        // CBCentralManager triggers centralManagerDidUpdateState when ready
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
            let days = entry.dayMask.bleString         // e.g. "Mon+Wed+Fri"
            let time = String(format: "%02d:%02d", entry.hour, entry.minute)
            let zone = "V\(entry.valve)"
            lines.append("SCHED:\(zone),\(days),\(time),\(entry.durationMin)")
        }
        lines.append("SCHED:END")

        // Small delay between writes so ESP32 has time to process each line
        for (index, line) in lines.enumerated() {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(index) * 0.15) {
                self.write(line, to: self.scheduleChar)
                print("BLE schedule tx: \(line)")
            }
        }
    }

    // ── Internal helpers ──────────────────────────────────────────────────────

    private func write(_ value: String, to characteristic: CBCharacteristic?) {
        guard let p = peripheral,
              let char = characteristic,
              let data = value.data(using: .utf8) else {
            print("BLE write failed — not connected or characteristic missing")
            return
        }
        p.writeValue(data, for: char, type: .withoutResponse)
    }

    // Parse STATUS string: "R:0,V1:0,V2:0,SCH:OK"
    private func parseStatus(_ value: String) {
        let parts = value.split(separator: ",")
        for part in parts {
            let kv = part.split(separator: ":")
            guard kv.count == 2 else { continue }
            let key = String(kv[0])
            let val = String(kv[1])
            switch key {
            case "R":   isRaining  = val == "1"
            case "V1":  valve1Open = val == "1"
            case "V2":  valve2Open = val == "1"
            case "SCH": scheduleAck = "SCH:\(val)"
            default:    break
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
        bleState = .disconnected
        statusChar  = nil
        scheduleChar = nil
        rainChar    = nil
        manualChar  = nil

        // Auto-reconnect after 2 seconds
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
                    [STATUS_UUID, SCHEDULE_UUID, RAIN_UUID, MANUAL_UUID],
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
                peripheral.setNotifyValue(true, for: char)   // subscribe to notifications
                peripheral.readValue(for: char)               // read current value immediately
            case SCHEDULE_UUID:
                scheduleChar = char
            case RAIN_UUID:
                rainChar = char
                peripheral.setNotifyValue(true, for: char)
            case MANUAL_UUID:
                manualChar = char
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
                self.isRaining = str == "R:1"
            default:
                break
            }
        }
    }
}
