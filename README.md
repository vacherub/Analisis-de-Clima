# Analisis-de-Clima v2.1

Análisis del clima, tiempo y otros parametros utiles para las capitales regionales de Italia. Disponible en **Python** (`forecast.py`), **C** (`c/forecast.c`, compilable en Windows, Linux y macOS) y como **app gráfica nativa para macOS** (`app-macos/`).

## Descripción

Genera un pronóstico meteorológico completo para las 20 capitales de región de Italia. Combina datos históricos observados, pronóstico de la API Open-Meteo y análisis estadístico para producir informes detallados.

## Ciudades disponibles

Las 20 capitales regionales de Italia: ANCONA, AOSTA, L'AQUILA, BARI, BOLOGNA, CAGLIARI, CAMPOBASSO, CATANZARO, FIRENZE, GENOVA, MILANO, NAPOLI, PALERMO, PERUGIA, POTENZA, ROMA, TORINO, TRENTO, TRIESTE, VENEZIA.

En el reporte, la ciudad se muestra con su región entre paréntesis, por ejemplo: `Torino (Piemonte)`.

Ver listado completo con `--list`.

## Uso (Python)

```
python forecast.py [--ciudad CIUDAD] [--dias N] [--extendido] [--today] [--resumen]
```

## Uso (C)

```
./forecast [--ciudad CIUDAD] [--dias N] [--extendido] [--today] [--resumen]
```

### Binarios precompilados

Hay binarios listos para ejecutar en `c/bin/` (compilados desde `c/forecast.c`):

| Archivo | Plataforma | Arquitectura |
|---------|------------|--------------|
| `c/bin/forecast_linux` | Linux | x86-64 |
| `c/bin/forecast_macos` | macOS (10.13+) | Universal (arm64 + x86-64) |
| `c/bin/forecast.exe` | Windows | x86-64 |

```bash
# Ejemplos de uso directo:
./c/bin/forecast_linux -c ROMA -d 5
./c/bin/forecast_macos -c MILANO -d 3
./c/bin/forecast.exe -c NAPOLI -t
```

Solo requieren libcurl en Linux (normalmente ya instalado); en macOS y Windows usan librerías del sistema, sin dependencias externas. Los binarios no se suben al repositorio; se generan con `make` (ver Compilación).

### App gráfica para macOS

Hay una app nativa (SwiftUI) en `app-macos/` que permite seleccionar la ciudad de una lista, elegir los días (1-16) y ver el pronóstico en una ventana, sin usar terminal.

**Requisito:** Xcode Command Line Tools (`xcode-select --install`) — no necesitas Xcode completo.

```bash
cd app-macos
./build.sh          # compila "Analisis de Clima.app" (universal arm64+x86_64) en dist/
./build.sh --open   # compila y la abre
```

Para ejecutar después: `open "dist/Analisis de Clima.app"`.

> Nota: al ser una app nativa de macOS (SwiftUI), no funciona en Windows/Linux; usa la versión Python o C para eso.

### Compilación (C)

La versión C usa **libcurl** (Linux/macOS) o **WinHTTP** integrado (Windows, sin dependencias externas) e incluye **cJSON** en `c/`. El Makefile detecta la plataforma automáticamente.

- **Linux**
  ```bash
  cd c
  make
  ```
  En Debian/Ubuntu instala primero: `sudo apt install libcurl4-openssl-dev`.

- **macOS**
  ```bash
  cd c
  make
  ```
  libcurl ya viene incluido en macOS (no requiere instalación). Para cruzar-compilar desde Linux con osxcross: `o64-clang -O2 -Wall -o forecast forecast.c cJSON.c -lcurl -lm` (y `oa64-clang` para arm64; combinar con `lipo` para universal).

- **Windows (MinGW / MSYS2)**
  ```bash
  cd c
  make
  # o directamente:
  gcc -O2 -Wall -o forecast.exe forecast.c cJSON.c -lwinhttp
  ```
  Usa WinHTTP (integrado en Windows), sin dependencias externas. Para cruzar-compilar desde Linux: `make CC=x86_64-w64-mingw32-gcc`.

- **Windows (MSVC)**
  Compila con `cl forecast.c cJSON.c /link winhttp.lib` (WinHTTP integrado).

### Parámetros

| Parámetro | Corto | Valor por defecto | Descripción |
|-----------|-------|-------------------|-------------|
| `--ciudad` | `-c` | TORINO | Ciudad a consultar |
| `--dias` | `-d` | 5 | Días de pronóstico (1-16) |
| `--extendido` | `-e` | — | Incluye predicción horaria detallada |
| `--list` | `-l` | — | Lista todas las ciudades disponibles |
| `--today` | `-t` | — | Muestra datos hora por hora del día actual |
| `--resumen` | `-r` | — | Muestra solo tabla resumen y resumen ejecutivo |

### Ejemplos

```bash
# Python
python forecast.py                        # Torino 5 días
python forecast.py -c ROMA                # Roma 5 días
python forecast.py --today                # Hoy Torino (hora por hora)
python forecast.py -t -c NAPOLI           # Hoy Napoli
python forecast.py -c MILANO -d 10        # Milano 10 días
python forecast.py -c NAPOLI -e           # Napoli + detalle horario
python forecast.py -l                     # Listar ciudades
python forecast.py -r                     # Solo resumen
python forecast.py -r -c MILANO -d 10     # Resumen Milano 10 días

# C (idénticos parámetros, usando ./forecast)
./forecast -r -c MILANO -d 10
```

## Estructura del proyecto

```
Analisis-de-Clima/
├── README.md              # Documentación
├── forecast.py            # Versión en Python
├── .gitignore
├── c/                     # Versión en C (multiplataforma)
│   ├── forecast.c         # Código principal
│   ├── cJSON.c            # Parser JSON (incluido)
│   ├── cJSON.h
│   ├── Makefile           # Detección automática de plataforma
│   ├── bin/               # Binarios precompilados (no versionados)
│   │   ├── forecast_linux
│   │   ├── forecast_macos
│   │   └── forecast.exe
│   └── .gitignore
└── app-macos/             # App gráfica nativa macOS (SwiftUI)
    ├── build.sh           # Compila "Analisis de Clima.app" en dist/
    ├── Sources/           # Código Swift
    │   ├── AnalisisDeClimaApp.swift
    │   ├── ContentView.swift    # Interfaz
    │   ├── Cities.swift         # Las 20 capitales regionales
    │   └── WeatherClient.swift  # Cliente Open-Meteo
    └── .gitignore
```

## Fuentes de datos

- **Open-Meteo API** — gratuita, sin API key requerida. Combina modelos ECMWF/GFS/ICON.
- **Normales climáticas 1991-2020** — temperaturas medias y precipitación de julio para cada ciudad.

## Proceso

1. **Fetch** — consulta a Open-Meteo (datos horarios y diarios de los últimos 7 días + días solicitados).
2. **Análisis histórico** — calcula anomalías térmicas respecto a la normal climática, tendencia reciente, rachas secas y detección de olas de calor.
3. **Pronóstico diario** — tabla resumen con temperaturas (mín/máx con hora), humedad, precipitación y condición dominante.
4. **Riesgo de zancudos** — índice 0-10 basado en temperatura (45%), humedad (35%) y precipitación (20%).
5. **Tormentas convectivas** — detección de eventos de tormenta según códigos WMO, con severidad y horario.
6. **Resumen ejecutivo** — recomendaciones según condiciones extremas (calor, tormentas, humedad, zancudos).
7. **Detalle horario** — (opcional con `--extendido`) despliegue hora por hora con temperatura, humedad, precipitación y condición.

## Salida

El reporte incluye:

1. Datos observados (histórico reciente)
2. Tabla resumen de los próximos días
3. Análisis de anomalías y tendencia
4. Índice de riesgo de zancudos
5. Análisis de tormentas convectivas
6. Resumen ejecutivo con recomendaciones
7. Predicción horaria detallada (opcional)

## History

- **v1.0** — Versión inicial
- **v2.0** — Nuevo parámetro `--resumen` / `-r` para mostrar solo tabla resumen y resumen ejecutivo (renumerados 1 y 2); eliminada BERGAMO y agregada ANCONA (las 20 capitales regionales); la ciudad se muestra con su región entre paréntesis; versión en C multiplataforma (`c/forecast.c` con libcurl/WinHTTP + cJSON); binarios precompilados para Linux, macOS y Windows en `c/bin/`
- **v2.1** — Corregido `--dias` en la versión C (ahora muestra hasta 16 días, antes solo 9); binarios reconstruidos y reorganizados en `c/bin/`; limpieza de artefactos de build; `.gitignore` para código fuente y binarios; nueva **app gráfica nativa para macOS** (`app-macos/`, SwiftUI) con selector de ciudad, días (1-16) y pronóstico en ventana
