# Analisis-de-Clima v1.0

Análisis del clima, tiempo y otros parametros utiles para ciudades de Italia.

## Descripción

Script en Python que genera un pronóstico meteorológico completo para las 20 capitales de región de Italia. Combina datos históricos observados, pronóstico de la API Open-Meteo y análisis estadístico para producir informes detallados.

## Ciudades disponibles

20 capitales de región: AOSTA, L'AQUILA, BARI, BERGAMO, BOLOGNA, CAGLIARI, CAMPOBASSO, CATANZARO, FIRENZE, GENOVA, MILANO, NAPOLI, PALERMO, PERUGIA, POTENZA, ROMA, TORINO, TRENTO, TRIESTE, VENEZIA.

Ver listado completo con `--list`.

## Uso

```
python forecast.py [--ciudad CIUDAD] [--dias N] [--extendido] [--today]
```

### Parámetros

| Parámetro | Corto | Valor por defecto | Descripción |
|-----------|-------|-------------------|-------------|
| `--ciudad` | `-c` | TORINO | Ciudad a consultar |
| `--dias` | `-d` | 5 | Días de pronóstico (1-16) |
| `--extendido` | `-e` | — | Incluye predicción horaria detallada |
| `--list` | `-l` | — | Lista todas las ciudades disponibles |
| `--today` | `-t` | — | Muestra datos hora por hora del día actual |

### Ejemplos

```bash
python forecast.py                        # Torino 5 días
python forecast.py -c ROMA                # Roma 5 días
python forecast.py --today                # Hoy Torino (hora por hora)
python forecast.py -t -c NAPOLI           # Hoy Napoli
python forecast.py -c MILANO -d 10        # Milano 10 días
python forecast.py -c NAPOLI -e           # Napoli + detalle horario
python forecast.py -l                     # Listar ciudades
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
