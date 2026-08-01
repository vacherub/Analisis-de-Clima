import SwiftUI

struct ContentView: View {
    @State private var selectedCity: City = Cities.all[15] // Roma
    @State private var days = 5
    @State private var forecast: [DailyForecast] = []
    @State private var isLoading = false
    @State private var errorMessage: String?
    @State private var lastUpdated: Date?

    private let client = WeatherClient()

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header
            Divider()
            controls
            Divider()
            content
        }
        .frame(minWidth: 640, minHeight: 480)
        .padding()
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("Pronóstico Meteorológico de Italia")
                .font(.title2.bold())
            Text("Análisis del clima para las 20 capitales regionales — Open-Meteo")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding(.bottom, 10)
    }

    private var controls: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Ciudad").font(.caption).foregroundColor(.secondary)
                Picker("", selection: $selectedCity) {
                    ForEach(Cities.all) { city in
                        Text(city.label).tag(city)
                    }
                }
                .labelsHidden()
                .frame(width: 240)
            }

            VStack(alignment: .leading, spacing: 2) {
                Text("Días").font(.caption).foregroundColor(.secondary)
                Picker("", selection: $days) {
                    ForEach(1...16, id: \.self) { n in
                        Text("\(n)").tag(n)
                    }
                }
                .labelsHidden()
                .frame(width: 70)
            }

            Button(action: load) {
                if isLoading {
                    ProgressView().controlSize(.small)
                } else {
                    Label("Consultar", systemImage: "cloud.sun.fill")
                }
            }
            .buttonStyle(.borderedProminent)
            .disabled(isLoading)

            Spacer()

            if let errorMessage {
                Text(errorMessage)
                    .font(.caption)
                    .foregroundColor(.red)
                    .lineLimit(2)
            } else if let lastUpdated {
                Text("Actualizado: \(lastUpdated.formatted(date: .omitted, time: .shortened))")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.vertical, 8)
    }

    @ViewBuilder
    private var content: some View {
        if isLoading {
            Spacer()
            ProgressView("Consultando Open-Meteo…")
                .controlSize(.large)
            Spacer()
        } else if forecast.isEmpty {
            Spacer()
            VStack(spacing: 8) {
                Image(systemName: "cloud.sun.fill")
                    .font(.system(size: 48))
                    .foregroundColor(.yellow)
                Text("Selecciona una ciudad y consulta el pronóstico")
                    .foregroundColor(.secondary)
            }
            Spacer()
        } else {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    citySummary
                    forecastTable
                    statsRow
                }
                .padding(.top, 8)
            }
        }
    }

    private var citySummary: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(selectedCity.label)
                    .font(.title3.bold())
                Text(String(format: "%.2f°N, %.2f°E | %d m snm | Normal julio: %.1f°C / %d mm",
                            selectedCity.lat, selectedCity.lon,
                            selectedCity.alt, selectedCity.tMean, Int(selectedCity.precip)))
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Spacer()
        }
    }

    private var forecastTable: some View {
        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 8) {
            GridRow {
                headerCell("Día")
                headerCell("T.Min", alignment: .trailing)
                headerCell("T.Max", alignment: .trailing)
                headerCell("H.Min", alignment: .trailing)
                headerCell("H.Max", alignment: .trailing)
                headerCell("Lluvia", alignment: .trailing)
                headerCell("Condición")
            }
            Divider()

            ForEach(forecast) { day in
                GridRow {
                    Text(day.dateLabel)
                        .frame(minWidth: 110, alignment: .leading)
                    cell(String(format: "%.1f°C (%02d:00)", day.tMin, day.tMinHour), trailing: true)
                    cell(String(format: "%.1f°C (%02d:00)", day.tMax, day.tMaxHour), trailing: true)
                    cell("\(day.hMin)%", trailing: true)
                    cell("\(day.hMax)%", trailing: true)
                    cell(String(format: "%.2f mm", day.precip), trailing: true)
                    Text("\(day.icon) \(day.condition)")
                        .frame(minWidth: 200, alignment: .leading)
                }
            }
        }
        .padding(12)
        .background(Color(nsColor: .textBackgroundColor))
        .cornerRadius(8)
    }

    private var statsRow: some View {
        let maxT = forecast.map(\.tMax).max() ?? 0
        let minT = forecast.map(\.tMin).min() ?? 0
        let totalP = forecast.reduce(0) { $0 + $1.precip }
        let avgT = forecast.map(\.tMax).reduce(0, +) / Double(max(forecast.count, 1))

        return HStack(spacing: 24) {
            statBox(label: "Máx absoluta", value: String(format: "%.1f°C", maxT))
            statBox(label: "Mín absoluta", value: String(format: "%.1f°C", minT))
            statBox(label: "T.Media máx", value: String(format: "%.1f°C", avgT))
            statBox(label: "Precip. total", value: String(format: "%.1f mm", totalP))
        }
    }

    private func statBox(label: String, value: String) -> some View {
        VStack(spacing: 2) {
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
            Text(value)
                .font(.headline)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(Color(nsColor: .controlBackgroundColor))
        .cornerRadius(8)
    }

    private func headerCell(_ text: String, alignment: Alignment = .leading) -> some View {
        Text(text)
            .font(.caption.bold())
            .foregroundColor(.secondary)
            .frame(maxWidth: .infinity, alignment: alignment)
    }

    private func cell(_ text: String, trailing: Bool = false) -> some View {
        Text(text)
            .font(.callout.monospacedDigit())
            .frame(minWidth: 90, alignment: trailing ? .trailing : .leading)
    }

    private func load() {
        isLoading = true
        errorMessage = nil
        Task {
            do {
                let result = try await client.fetch(city: selectedCity, days: days)
                await MainActor.run {
                    forecast = result
                    lastUpdated = Date()
                    isLoading = false
                }
            } catch {
                await MainActor.run {
                    errorMessage = error.localizedDescription
                    forecast = []
                    isLoading = false
                }
            }
        }
    }
}
