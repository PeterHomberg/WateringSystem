// ScheduleView.swift
import SwiftUI
import SwiftUI

struct ScheduleView: View {
    @EnvironmentObject var ble: BLEManager
    @State private var entries: [ScheduleEntry] = []
    @State private var showingAddEntry = false
    @State private var editingEntry: ScheduleEntry? = nil
    @State private var showingSendConfirm = false
    @State private var sendResult: String = ""

    var body: some View {
        NavigationView {
            VStack(spacing: 0) {

                // ── Send status banner ────────────────────────────────────
                if !sendResult.isEmpty {
                    HStack(spacing: 8) {
                        Image(systemName: sendResult == "SCH:OK"
                              ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundColor(sendResult == "SCH:OK" ? .green : .red)
                        Text(sendResult == "SCH:OK"
                             ? "Schedule saved to controller"
                             : "Error saving schedule — try again")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                        Spacer()
                        Button(action: { sendResult = "" }) {
                            Image(systemName: "xmark")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding()
                    .background(sendResult == "SCH:OK"
                                ? Color.green.opacity(0.1)
                                : Color.red.opacity(0.1))
                }

                // ── Entry list ────────────────────────────────────────────
                if entries.isEmpty {
                    Spacer()
                    VStack(spacing: 12) {
                        Image(systemName: "calendar.badge.plus")
                            .font(.system(size: 48))
                            .foregroundColor(.secondary)
                        Text("No schedule entries")
                            .font(.headline)
                            .foregroundColor(.secondary)
                        Text("Tap + to add a watering time")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    Spacer()
                } else {
                    List {
                        ForEach(1...2, id: \.self) { zone in
                            let zoneEntries = entries.filter { $0.valve == zone }
                            if !zoneEntries.isEmpty {
                                Section(header: Text("Zone \(zone)")) {
                                    ForEach(zoneEntries) { entry in
                                        ScheduleRow(entry: entry)
                                            .contentShape(Rectangle())
                                            .onTapGesture {
                                                editingEntry = entry
                                            }
                                    }
                                    .onDelete { indexSet in
                                        deleteEntries(zone: zone, at: indexSet)
                                    }
                                }
                            }
                        }
                    }
                    .listStyle(InsetGroupedListStyle())
                }

                // ── Send to controller button ─────────────────────────────
                if !entries.isEmpty {
                    Button(action: { showingSendConfirm = true }) {
                        HStack(spacing: 8) {
                            Image(systemName: "arrow.up.circle.fill")
                            Text("Send to Controller")
                                .fontWeight(.semibold)
                        }
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 16)
                        .background(ble.bleState == .connected ? Color.blue : Color.gray)
                        .foregroundColor(.white)
                        .cornerRadius(14)
                        .padding()
                    }
                    .disabled(ble.bleState != .connected)
                }
            }
            .navigationTitle("Schedule")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: { showingAddEntry = true }) {
                        Image(systemName: "plus")
                    }
                    .disabled(entries.count >= 6)
                }
            }
            .sheet(isPresented: $showingAddEntry) {
                ScheduleEntryEditor(entry: nil) { newEntry in
                    entries.append(newEntry)
                }
            }
            .sheet(item: $editingEntry) { entry in
                ScheduleEntryEditor(entry: entry) { updated in
                    if let idx = entries.firstIndex(where: { $0.id == updated.id }) {
                        entries[idx] = updated
                    }
                }
            }
            .alert(isPresented: $showingSendConfirm) {
                Alert(
                    title: Text("Send Schedule"),
                    message: Text("This will replace the current schedule on the controller."),
                    primaryButton: .default(Text("Send")) {
                        sendSchedule()
                    },
                    secondaryButton: .cancel()
                )
            }
            .onReceive(ble.$scheduleAck) { ack in
                if ack == "SCH:OK" || ack == "SCH:ERR" {
                    sendResult = ack
                }
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private func deleteEntries(zone: Int, at indexSet: IndexSet) {
        let zoneEntries = entries.filter { $0.valve == zone }
        let toDelete = indexSet.map { zoneEntries[$0].id }
        entries.removeAll { toDelete.contains($0.id) }
    }

    private func sendSchedule() {
        sendResult = ""
        ble.sendSchedule(entries.filter { $0.enabled })
    }
}

// ── Schedule row ──────────────────────────────────────────────────────────────
struct ScheduleRow: View {
    let entry: ScheduleEntry

    var body: some View {
        HStack(spacing: 14) {
            VStack(alignment: .leading, spacing: 4) {
                Text(entry.timeString)
                    .font(.headline)
                Text(entry.dayMask.displayString)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            Spacer()
            VStack(alignment: .trailing, spacing: 4) {
                Text(entry.durationString)
                    .font(.subheadline)
                    .foregroundColor(.blue)
                Image(systemName: "drop.fill")
                    .foregroundColor(.blue)
                    .font(.caption)
            }
        }
        .padding(.vertical, 4)
        .opacity(entry.enabled ? 1.0 : 0.4)
    }
}

// ── Add / Edit entry sheet ────────────────────────────────────────────────────
struct ScheduleEntryEditor: View {
    @Environment(\.presentationMode) var presentationMode

    let entry: ScheduleEntry?
    let onSave: (ScheduleEntry) -> Void

    @State private var valve:       Int     = 1
    @State private var hour:        Int     = 7
    @State private var minute:      Int     = 0
    @State private var durationMin: Int     = 20
    @State private var dayMask:     DayMask = .weekdays
    @State private var enabled:     Bool    = true

    init(entry: ScheduleEntry?, onSave: @escaping (ScheduleEntry) -> Void) {
        self.entry  = entry
        self.onSave = onSave
        if let e = entry {
            _valve       = State(initialValue: e.valve)
            _hour        = State(initialValue: e.hour)
            _minute      = State(initialValue: e.minute)
            _durationMin = State(initialValue: e.durationMin)
            _dayMask     = State(initialValue: e.dayMask)
            _enabled     = State(initialValue: e.enabled)
        }
    }

    var body: some View {
        NavigationView {
            Form {
                // Zone picker
                Section(header: Text("Zone")) {
                    Picker("Zone", selection: $valve) {
                        Text("Zone 1").tag(1)
                        Text("Zone 2").tag(2)
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }

                // Time picker
                Section(header: Text("Start Time")) {
                    HStack {
                        Picker("Hour", selection: $hour) {
                            ForEach(0..<24, id: \.self) { h in
                                Text(String(format: "%02d", h)).tag(h)
                            }
                        }
                        .pickerStyle(WheelPickerStyle())
                        .frame(width: 80)
                        .clipped()

                        Text(":")
                            .font(.title2)
                            .fontWeight(.semibold)

                        Picker("Minute", selection: $minute) {
                            ForEach(0..<59, id: \.self) { m in
                                Text(String(format: "%02d", m)).tag(m)
                            }
                        }
                        .pickerStyle(WheelPickerStyle())
                        .frame(width: 80)
                        .clipped()
                    }
                    .frame(height: 120)
                }

                // Duration
                Section(header: Text("Duration: \(durationMin) minutes")) {
                    Slider(value: Binding(
                        get: { Double(durationMin) },
                        set: { durationMin = Int($0) }
                    ), in: 1...120, step: 1)
                }

                // Days of week
                Section(header: Text("Days")) {
                    ForEach(DayMask.ordered, id: \.1) { day, name in
                        HStack {
                            Text(name)
                            Spacer()
                            if dayMask.contains(day) {
                                Image(systemName: "checkmark")
                                    .foregroundColor(.blue)
                            }
                        }
                        .contentShape(Rectangle())
                        .onTapGesture {
                            if dayMask.contains(day) {
                                dayMask.remove(day)
                            } else {
                                dayMask.insert(day)
                            }
                        }
                    }
                }

                // Enabled toggle
                Section {
                    Toggle("Enabled", isOn: $enabled)
                }
            }
            .navigationTitle(entry == nil ? "Add Entry" : "Edit Entry")
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        presentationMode.wrappedValue.dismiss()
                    }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Save") {
                        let saved = ScheduleEntry(
                            id:          entry?.id ?? UUID(),
                            valve:       valve,
                            dayMask:     dayMask,
                            hour:        hour,
                            minute:      minute,
                            durationMin: durationMin,
                            enabled:     enabled
                        )
                        onSave(saved)
                        presentationMode.wrappedValue.dismiss()
                    }
                    .disabled(dayMask.rawValue == 0)
                }
            }
        }
    }
}

// ── Preview ───────────────────────────────────────────────────────────────────
struct ScheduleView_Previews: PreviewProvider {
    static var previews: some View {
        ScheduleView()
            .environmentObject(BLEManager())
    }
}
