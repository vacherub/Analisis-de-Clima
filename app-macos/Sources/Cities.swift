import Foundation

struct City: Identifiable, Hashable {
    let id = UUID()
    let key: String
    let name: String
    let region: String
    let lat: Double
    let lon: Double
    let alt: Int
    let tMean: Double
    let precip: Double

    var label: String { "\(name) (\(region))" }
}

enum Cities {
    static let all: [City] = [
        City(key: "ANCONA",    name: "Ancona",    region: "Marche",               lat: 43.62, lon: 13.52, alt: 16,  tMean: 24.0, precip: 30),
        City(key: "AOSTA",     name: "Aosta",     region: "Valle d'Aosta",        lat: 45.74, lon: 7.32,  alt: 583, tMean: 21.0, precip: 45),
        City(key: "AQUILA",    name: "L'Aquila",  region: "Abruzzo",              lat: 42.35, lon: 13.40, alt: 714, tMean: 21.5, precip: 35),
        City(key: "BARI",      name: "Bari",      region: "Puglia",               lat: 41.12, lon: 16.87, alt: 5,   tMean: 26.5, precip: 20),
        City(key: "BOLOGNA",   name: "Bologna",   region: "Emilia-Romagna",       lat: 44.49, lon: 11.34, alt: 54,  tMean: 25.0, precip: 40),
        City(key: "CAGLIARI",  name: "Cagliari",  region: "Sardegna",             lat: 39.22, lon: 9.12,  alt: 4,   tMean: 26.5, precip: 3),
        City(key: "CAMPOBASSO",name: "Campobasso",region: "Molise",               lat: 41.56, lon: 14.66, alt: 701, tMean: 22.5, precip: 30),
        City(key: "CATANZARO", name: "Catanzaro", region: "Calabria",             lat: 38.91, lon: 16.60, alt: 342, tMean: 25.0, precip: 10),
        City(key: "FIRENZE",   name: "Firenze",   region: "Toscana",              lat: 43.77, lon: 11.26, alt: 50,  tMean: 25.0, precip: 40),
        City(key: "GENOVA",    name: "Genova",    region: "Liguria",              lat: 44.41, lon: 8.93,  alt: 19,  tMean: 24.5, precip: 30),
        City(key: "MILANO",    name: "Milano",    region: "Lombardia",            lat: 45.46, lon: 9.19,  alt: 122, tMean: 24.0, precip: 65),
        City(key: "NAPOLI",    name: "Napoli",    region: "Campania",             lat: 40.85, lon: 14.27, alt: 17,  tMean: 26.0, precip: 25),
        City(key: "PALERMO",   name: "Palermo",   region: "Sicilia",              lat: 38.12, lon: 13.36, alt: 14,  tMean: 27.0, precip: 5),
        City(key: "PERUGIA",   name: "Perugia",   region: "Umbria",               lat: 43.11, lon: 12.39, alt: 493, tMean: 24.0, precip: 35),
        City(key: "POTENZA",   name: "Potenza",   region: "Basilicata",           lat: 40.64, lon: 15.80, alt: 819, tMean: 22.0, precip: 25),
        City(key: "ROMA",      name: "Roma",      region: "Lazio",                lat: 41.90, lon: 12.50, alt: 21,  tMean: 25.5, precip: 20),
        City(key: "TORINO",    name: "Torino",    region: "Piemonte",             lat: 45.07, lon: 7.67,  alt: 239, tMean: 23.2, precip: 56),
        City(key: "TRENTO",    name: "Trento",    region: "Trentino-Alto Adige",  lat: 46.07, lon: 11.12, alt: 190, tMean: 22.5, precip: 70),
        City(key: "TRIESTE",   name: "Trieste",   region: "Friuli-Venezia Giulia",lat: 45.65, lon: 13.77, alt: 2,   tMean: 24.5, precip: 65),
        City(key: "VENEZIA",   name: "Venezia",   region: "Veneto",               lat: 45.44, lon: 12.32, alt: 1,   tMean: 24.5, precip: 50),
    ].sorted { $0.name < $1.name }
}
