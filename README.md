# Analisis-de-Clima v2.4

Análisis del clima, tiempo y otros parámetros útiles para las capitales de región y de provincia de Italia, **en C** (compilable en Windows, Linux y macOS).

> La versión en **Python** fue eliminada. Este proyecto mantiene **solo la versión en C**.

## Descripción

Genera un pronóstico meteorológico completo para las capitales de región y de provincia de Italia. Combina datos históricos observados, pronóstico de la API Open-Meteo y análisis estadístico para producir informes detallados.

## Configuración: `clima.conf`

El archivo **`clima.conf`** define **las ciudades disponibles, sus coordenadas y su región**, y cuál es la ciudad por defecto. El estilo es **`NOMBRE;lat,lon;REGION`** (una ciudad por línea):

```ini
#CAMPOBASSO;41.56,14.66;Molise
#FIRENZE;43.77,11.26;Toscana
#MILANO;45.46,9.19;Lombardia
TORINO;45.07,7.67;Piemonte
```

- La **única línea sin `#`** es la ciudad por defecto (aquí TORINO).
- El programa, al ejecutarse, **lee la ciudad, sus coordenadas y su región del archivo** y las usa para consultar a Open-Meteo.
- **`--list` / `-l`** lista las ciudades definidas en el archivo.
- **Para añadir una ciudad nueva**: agrega una línea `NOMBRE;lat,lon;REGION`, p. ej. `BOLZANO;46.49,11.36;Trentino-Alto Adige`.
- **La región es opcional**: `NOMBRE;lat,lon` también es válido.
- **Para cambiar la por defecto**: quita el `#` a la que quieras y comenta la actual (deja solo una línea sin `#`).

Para las 20 capitales conocidas, el programa completa nombre visible, altitud y normales automáticamente. Para una ciudad nueva, usa el nombre, coordenadas y región del propio archivo. El programa muestra conjuntamente **`Ciudad (Región, coordenadas)`** en sus reportes.

El archivo se busca en el directorio actual y, si no está, junto al ejecutable (para que los binarios funcionen desde cualquier carpeta). Si no hay `clima.conf`, el programa usa por defecto TORINO con las 20 capitales de región.

## Uso

```
./forecast [--ciudad CIUDAD] [--dias DIAS] [--extendido] [--today] [--resumen] [--list] [--version]
```

### Binarios precompilados

| Archivo | Plataforma | Arquitectura |
|---------|------------|--------------|
| `forecast` | Linux | x86-64 |
| `forecast_macos` | macOS (11+) | Universal (arm64 + x86-64) |
| `forecast.exe` | Windows | x86-64 |

```bash
# Ejemplos de uso directo:
./forecast -c ROMA -d 5
./forecast_macos -c MILANO -d 3
./forecast.exe -c NAPOLI -t
```

Solo requieren libcurl en Linux (normalmente ya instalado); en macOS y Windows usan librerías del sistema, sin dependencias externas. Los binarios no se suben al repositorio; se generan con `make` (ver Compilación).

## Parámetros

| Parámetro | Corto | Valor por defecto | Descripción |
|-----------|-------|-------------------|-------------|
| `--ciudad` | `-c` | la de `clima.conf` (TORINO si no hay) | Ciudad a consultar |
| `--dias` | `-d` | 5 | Días de pronóstico (1-16) |
| `--extendido` | `-e` | — | Incluye predicción horaria detallada |
| `--today` | `-t` | — | Muestra datos hora por hora del día de hoy |
| `--resumen` | `-r` | — | Muestra solo tabla resumen y resumen ejecutivo |
| `--list` | `-l` | — | Lista las ciudades definidas en `clima.conf` |
| `--version` | `-v` | — | Muestra la versión del programa |
| `--help` | `-h` | — | Muestra la ayuda |

## Ejemplos

```bash
./forecast                        # ciudad por defecto (de clima.conf), 5 días
./forecast -v                     # versión
./forecast -l                     # listar ciudades (de clima.conf)
./forecast -c ROMA                # Roma 5 días
./forecast --today                # hoy (hora por hora)
./forecast -t -c NAPOLI           # hoy Napoli
./forecast -c MILANO -d 10        # Milano 10 días
./forecast -c NAPOLI -e           # Napoli + detalle horario
./forecast -r                     # solo resumen
./forecast -r -c MILANO -d 10     # resumen Milano 10 días
```

## Compilación

La versión C usa **libcurl** (Linux/macOS) o **WinHTTP** integrado (Windows, sin dependencias externas) e incluye **cJSON**. El `Makefile` detecta la plataforma automáticamente.

- **Linux**
  ```bash
  make
  ```
  En Debian/Ubuntu instala primero: `sudo apt install libcurl4-openssl-dev`.

- **macOS**
  ```bash
  make
  ```
  libcurl ya viene incluido en macOS. Para cruzar-compilar desde Linux con osxcross: `o64-clang -O2 -Wall -o forecast forecast.c cJSON.c -lcurl -lm` (y `oa64-clang` para arm64; combinar con `lipo` para universal).

- **Windows (MinGW / MSYS2)**
  ```bash
  make
  # o directamente:
  gcc -O2 -Wall -o forecast.exe forecast.c cJSON.c -lwinhttp
  ```
  Usa WinHTTP (integrado), sin dependencias externas. Para cruzar desde Linux: `make CC=x86_64-w64-mingw32-gcc`.

- **Windows (MSVC)**
  ```
  cl forecast.c cJSON.c /link winhttp.lib
  ```

## Estructura del proyecto

```
Analisis-de-Clima/
├── README.md              # Documentación
├── Makefile               # Compilación multiplataforma
├── forecast.c             # Código principal (C)
├── cJSON.c                # Parser JSON (incluido)
├── cJSON.h
├── clima.conf             # Configuración: ciudades + coordenadas + región + por defecto
├── .gitignore
└── Binarios (no versionados, generados con make):
    forecast, forecast_macos, forecast.exe
```

## Fuentes de datos

- **Open-Meteo API** — gratuita, sin API key requerida. Combina modelos ECMWF/GFS/ICON.
- **Normales climáticas medias anuales 1991-2020** — temperatura media y precipitación media diaria para las 20 capitales conocidas (usadas como referencia base para el cálculo de anomalías).
## Proceso

1. **Fetch** — consulta a Open-Meteo (datos horarios y diarios de los últimos 7 días + días solicitados), usando lat/lon desde `clima.conf`. Incluye `apparent_temperature`, viento, ráfagas, dirección, índice UV y salida/puesta de sol.
2. **Análisis histórico** — anomalías térmicas respecto a la normal, tendencia reciente, rachas secas y olas de calor.
3. **Pronóstico diario** — tabla con temperaturas (mín/máx con hora), humedad, precipitación, condición dominante, **índice UV** y **viento** (máx. del día con dirección).
4. **Riesgo de mosquitos** — índice 0-10 (temperatura 45%, humedad 35%, precipitación 20%).
5. **Tormentas convectivas** — detección por códigos WMO, con severidad y horario.
6. **Resumen ejecutivo** — recomendaciones ante condiciones extremas.
7. **Detalle horario** — (opcional con `--extendido`) hora por hora con temperatura, sensación térmica, humedad, lluvia, viento y UV.

## Salida

La salida usa **tipografía de ancho fijo** para que las tablas queden alineadas:

- **Windows**: el programa fuerza en la consola una fuente monoespaciada del sistema (`Cascadia Mono`, `Consolas` o `Courier New`), así la salida se ve bien incluso en la consola clásica de `cmd`.
- **macOS**: el emulador de terminal ya usa por defecto una fuente de ancho fijo (**Menlo**), sin configurar nada.
- **Linux**: los emuladores de terminal usan por defecto una fuente monoespaciada (p. ej. `DejaVu Sans Mono`), también sin configurar.

El programa no puede cambiar la fuente dentro de un emulador de terminal (Linux/macOS); en Windows sí lo hace automáticamente.

Contenido de la salida:

1. Datos observados (histórico reciente)
2. Tabla resumen de los próximos días (con UV y viento)
3. Análisis de anomalías y tendencia
4. Índice de riesgo de mosquitos
5. Análisis de tormentas convectivas
6. Resumen ejecutivo con recomendaciones
7. Predicción horaria detallada (opcional; con sensación térmica, viento y UV)

En `--today` y `--extendido` el programa incluye además: **sensación térmica** (apparent temperature), **viento** (velocidad, ráfagas y dirección), **salida y puesta de sol**, e **índice UV** (con avisos si es alto/extremo).

## Robustez de red

El programa verifica la respuesta de Open-Meteo antes de mostrar resultados:

- **Código HTTP**: si la API responde con un error 4xx/5xx (p. ej. 429 «too many requests» o 400 por parámetros inválidos), se avisa y no se muestran datos vacíos.
- **Errores JSON**: Open-Meteo puede devolver `{"error":true,"reason":"..."}` con HTTP 200; el programa lo detecta y muestra el motivo.
- **Reintentos**: ante un fallo de red o un código HTTP erróneo se realiza un **segundo intento** (2 s de espera) antes de abortar.

## History

- **v1.0** — Versión inicial (Python).
- **v2.0** — Solo versión en C (multiplataforma: Windows/Linux/macOS); eliminada la versión Python y la app `app-macos/`; código movido a la raíz del proyecto; nuevo archivo de configuración **`clima.conf`** con formato `NOMBRE;lat,lon` que define la lista de ciudades, sus coordenadas y la ciudad por defecto (TORINO); `--list` lee del archivo y permite añadir ciudades nuevas; nuevo parámetro **`--version` / `-v`**.
- **v2.1** — La ayuda (`-h` / `--help`) ahora incluye una sección **"Configuración: clima.conf"** con instrucciones para modificar el archivo (añadir o quitar ciudades y cómo obtener sus coordenadas para agregarlas). Limpieza del repositorio y ejecutables añadidos a `.gitignore`.
- **v2.2** — `clima.conf` incluye las **110 capitales de provincia de Italia** con el nuevo formato **`NOMBRE;lat,lon;REGION`** (la región es opcional). El programa ahora muestra conjuntamente **`Ciudad (Región, coordenadas)`** en sus reportes, tomando la región del propio archivo. La salida usa **tipografía de ancho fijo**: en Windows el programa fuerza una fuente monoespaciada del sistema (Cascadia Mono/Consolas); en macOS y Linux la usa la del emulador de terminal (Menlo, DejaVu Sans Mono...).
- **v2.3** — Nuevas métricas meteorológicas en todos los reportes: **viento** (velocidad, ráfagas y dirección, en `--today`, `-e` y la tabla resumen), **sensación térmica** (apparent temperature), **salida y puesta de sol** e **índice UV** (con aviso si es alto/extremo). **Robustez de red**: el programa ahora verifica el **código HTTP** de la respuesta de Open-Meteo y detecta los **errores JSON** (`{"error":true,...}`) evitando datos vacíos; **reintenta** automáticamente (2º intento tras 2 s) ante fallos de red o HTTP erróneo. Corrección del User-Agent de Windows (ahora anuncia la versión actual) y factorización del cliente HTTP para ambas plataformas.
- **v2.4** — Corrección de anomalías térmicas: las normales climáticas almacenadas en `METADATA` pasan de ser **medias de julio** a ser **medias anuales** (1991-2020), evitando comparaciones engañosas contra la temperatura del mes actual. Actualizadas las etiquetas `Normal julio` → `Normal climática` en todas las salidas y documentación.