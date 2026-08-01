import Foundation

// MARK: - Modelos de la API Open-Meteo

struct OpenMeteoResponse: Decodable {
    let daily: DailyData
    let hourly: HourlyData
}

struct DailyData: Decodable {
    let time: [String]
    let temperature2mMax: [Double]
    let temperature2mMin: [Double]
    let precipitationSum: [Double]
    let precipitationProbabilityMax: [Double]?

    enum CodingKeys: String, CodingKey {
        case time
        case temperature2mMax = "temperature_2m_max"
        case temperature2mMin = "temperature_2m_min"
        case precipitationSum = "precipitation_sum"
        case precipitationProbabilityMax = "precipitation_probability_max"
    }
}

struct HourlyData: Decodable {
    let time: [String]
    let temperature2m: [Double]
    let relativeHumidity2m: [Double]
    let precipitation: [Double]
    let weatherCode: [Int]

    enum CodingKeys: String, CodingKey {
        case time
        case temperature2m = "temperature_2m"
        case relativeHumidity2m = "relative_humidity_2m"
        case precipitation
        case weatherCode = "weather_code"
    }
}

// MARK: - Modelo de pronóstico por día

struct DailyForecast: Identifiable {
    let id = UUID()
    let date: Date
    let dateLabel: String
    let dayName: String
    let tMin: Double
    let tMax: Double
    let tMinHour: Int
    let tMaxHour: Int
    let hMin: Int
    let hMax: Int
    let precip: Double
    let condition: String
    let icon: String
}

// MARK: - Cliente HTTP

enum WeatherError: LocalizedError {
    case invalidURL
    case network(String)
    case decoding(String)

    var errorDescription: String? {
        switch self {
        case .invalidURL: return "URL inválida"
        case .network(let m): return "Error de red: \(m)"
        case .decoding(let m): return "Error al procesar datos: \(m)"
        }
    }
}

final class WeatherClient {
    func fetch(city: City, days: Int) async throws -> [DailyForecast] {
        let calendar = Calendar.current
        let today = calendar.startOfDay(for: Date())
        let start = calendar.date(byAdding: .day, value: -7, to: today)!
        let end = calendar.date(byAdding: .day, value: days - 1, to: today)!

        let df = DateFormatter()
        df.dateFormat = "yyyy-MM-dd"
        df.timeZone = TimeZone(identifier: "Europe/Rome")

        let startStr = df.string(from: start)
        let endStr = df.string(from: end)

        var urlStr = "https://api.open-meteo.com/v1/forecast"
        urlStr += "?latitude=\(city.lat)&longitude=\(city.lon)"
        urlStr += "&timezone=Europe/Rome"
        urlStr += "&start_date=\(startStr)&end_date=\(endStr)"
        urlStr += "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max"
        urlStr += "&hourly=temperature_2m,relative_humidity_2m,precipitation_probability,precipitation,weather_code,surface_pressure"
        urlStr += "&models=best_match"

        guard let url = URL(string: urlStr) else { throw WeatherError.invalidURL }

        let (data, response) = try await URLSession.shared.data(from: url)
        if let http = response as? HTTPURLResponse, http.statusCode != 200 {
            throw WeatherError.network("HTTP \(http.statusCode)")
        }

        let decoded: OpenMeteoResponse
        do {
            decoded = try JSONDecoder().decode(OpenMeteoResponse.self, from: data)
        } catch {
            throw WeatherError.decoding(error.localizedDescription)
        }

        return Self.buildDaily(decoded, today: today, days: days)
    }

    private static func buildDaily(_ resp: OpenMeteoResponse, today: Date, days: Int) -> [DailyForecast] {
        // Fecha objetivo de hoy en Europa/Rome
        var cal = Calendar.current
        cal.timeZone = TimeZone(identifier: "Europe/Rome")!
        let todayRome = cal.startOfDay(for: Date())

        var out: [DailyForecast] = []
        for i in 0..<resp.daily.time.count {
            guard let date = parseDate(resp.daily.time[i]) else { continue }
            guard date >= todayRome else { continue }
            if out.count >= days { break }

            let df = DateFormatter()
            df.dateFormat = "EEE d MMM"
            df.locale = Locale(identifier: "it_IT")
            df.timeZone = TimeZone(identifier: "Europe/Rome")
            let label = df.string(from: date)

            // Estadísticas del día desde datos horarios
            var tMin = Double.greatestFiniteMagnitude, tMax = -Double.greatestFiniteMagnitude
            var tMinH = 0, tMaxH = 0
            var hMin = 101, hMax = -1
            var precip = 0.0
            var codes: [Int] = []

            let isoPrefix = resp.daily.time[i]
            for j in 0..<resp.hourly.time.count {
                guard resp.hourly.time[j].hasPrefix(isoPrefix) else { continue }
                let hour = hourOf(resp.hourly.time[j])
                let t = resp.hourly.temperature2m[j]
                if t < tMin { tMin = t; tMinH = hour }
                if t > tMax { tMax = t; tMaxH = hour }
                let h = Int(resp.hourly.relativeHumidity2m[j])
                if h < hMin { hMin = h }
                if h > hMax { hMax = h }
                precip += resp.hourly.precipitation[j]
                codes.append(resp.hourly.weatherCode[j])
            }

            if tMin == Double.greatestFiniteMagnitude {
                tMin = resp.daily.temperature2mMin[i]
                tMax = resp.daily.temperature2mMax[i]
            }

            let (icon, cond) = condition(for: codes, dominant: dominantCode(codes))
            out.append(DailyForecast(
                date: date,
                dateLabel: label,
                dayName: dayName(from: date),
                tMin: tMin, tMax: tMax,
                tMinHour: tMinH, tMaxHour: tMaxH,
                hMin: hMin, hMax: hMax,
                precip: precip,
                condition: cond,
                icon: icon
            ))
        }
        return out
    }

    private static func parseDate(_ s: String) -> Date? {
        let df = DateFormatter()
        df.dateFormat = "yyyy-MM-dd"
        df.timeZone = TimeZone(identifier: "Europe/Rome")
        return df.date(from: s)
    }

    private static func hourOf(_ iso: String) -> Int {
        // Formato: "2026-07-31T14:00"
        let parts = iso.split(separator: "T")
        guard parts.count == 2, let hh = Int(parts[1].prefix(2)) else { return 0 }
        return hh
    }

    private static func dayName(from date: Date) -> String {
        let df = DateFormatter()
        df.dateFormat = "EEEE"
        df.locale = Locale(identifier: "it_IT")
        df.timeZone = TimeZone(identifier: "Europe/Rome")
        return df.string(from: date)
    }

    private static func dominantCode(_ codes: [Int]) -> Int {
        guard !codes.isEmpty else { return 0 }
        var counts: [Int: Int] = [:]
        for c in codes { counts[c, default: 0] += 1 }
        return counts.max { $0.value < $1.value }?.key ?? 0
    }

    private static func condition(for codes: [Int], dominant: Int) -> (String, String) {
        // Códigos WMO
        let thunder = codes.contains { [95, 96, 97, 99].contains($0) }
        let rain = codes.contains { [51, 53, 55, 61, 63, 65, 80, 81, 82].contains($0) }
        let drizzle = codes.contains { [56, 57].contains($0) }
        let snow = codes.contains { [71, 73, 75, 77, 85, 86].contains($0) }
        let fog = codes.contains { [45, 48].contains($0) }
        let overcast = codes.contains { [3].contains($0) }
        let partly = codes.contains { [2].contains($0) }

        if thunder { return ("⛈", "Tormentas aisladas") }
        if rain { return ("🌧", "Lluvias, parcialmente nublado") }
        if drizzle { return ("🌧", "Llovizna") }
        if snow { return ("🌨", "Nieve") }
        if fog { return ("🌫", "Niebla") }
        if overcast { return ("☁", "Nublado") }
        if partly { return ("⛅", "Parcialmente nublado") }

        switch dominant {
        case 0, 1: return ("☀", "Despejado")
        case 2: return ("⛅", "Parcialmente nublado")
        case 3: return ("☁", "Nublado")
        default: return ("❓", "Desconocido")
        }
    }
}
