/*
 * Pronóstico meteorológico para ciudades de Italia v2.2 (C)
 * Basado en: datos históricos observados + forecast Open-Meteo + análisis estadístico
 * Compila en Windows, Linux y macOS.
 *
 * Requiere: libcurl (Linux/macOS) o WinHTTP integrado (Windows) + cJSON (incluido).
 *
 * Salida con tipografía de ancho fijo: en Linux/macOS la usa la que elija el
 * emulador de terminal (Menlo, DejaVu Sans Mono...); en Windows se fuerza una
 * fuente monoespaciada del sistema (Cascadia Mono / Consolas).
 *
 * Uso: forecast [-c CIUDAD] [-d DIAS] [-e] [-l] [-t] [-r] [-v]
 *
 * Build Linux/macOS:  make
 * Build Windows:      gcc -O2 -Wall -o forecast.exe forecast.c cJSON.c -lwinhttp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "cJSON.h"

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
#include <winhttp.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <curl/curl.h>
#endif

#define MAX_HOURLY    (24 * 32)
#define MAX_DAYS      16
#define HIST_DAYS     7
#define URL_LEN       1200
#define MAX_CITIES    160
#define CONFIG_FILE   "clima.conf"
#define CONF_LINE     160
#define VERSIONA      "2.2"

/* ------------------------------------------------------------------ */
/* Metadatos de las 20 capitales (nombre visible, región, altitud,     */
/* normales julio 1991-2020). La lista de ciudades y sus coordenadas   */
/* viene de clima.conf; aquí sólo se busca el nombre para enriquecerla.*/
/* ------------------------------------------------------------------ */
typedef struct {
    const char *key;          /* nombre en clima.conf (clave)          */
    const char *name;         /* nombre visible                        */
    const char *region;
    double lat;
    double lon;
    int    alt;
    double t_mean;
    int    precip;
} MetaCiudad;

static const MetaCiudad METADATA[] = {
    {"ANCONA",    "Ancona",    "Marche",                 43.62, 13.52,  16, 24.0, 30},
    {"AOSTA",     "Aosta",     "Valle d'Aosta",          45.74,  7.32, 583, 21.0, 45},
    {"AQUILA",    "L'Aquila",  "Abruzzo",               42.35, 13.40, 714, 21.5, 35},
    {"BARI",      "Bari",      "Puglia",                 41.12, 16.87,   5, 26.5, 20},
    {"BOLOGNA",   "Bologna",   "Emilia-Romagna",         44.49, 11.34,  54, 25.0, 40},
    {"CAGLIARI",  "Cagliari",  "Sardegna",               39.22,  9.12,   4, 26.5,  3},
    {"CAMPOBASSO","Campobasso","Molise",                41.56, 14.66, 701, 22.5, 30},
    {"CATANZARO", "Catanzaro", "Calabria",              38.91, 16.60, 342, 25.0, 10},
    {"FIRENZE",   "Firenze",   "Toscana",               43.77, 11.26,  50, 25.0, 40},
    {"GENOVA",    "Genova",    "Liguria",               44.41,  8.93,  19, 24.5, 30},
    {"MILANO",    "Milano",    "Lombardia",             45.46,  9.19, 122, 24.0, 65},
    {"NAPOLI",    "Napoli",    "Campania",              40.85, 14.27,  17, 26.0, 25},
    {"PALERMO",   "Palermo",   "Sicilia",               38.12, 13.36,  14, 27.0,  5},
    {"PERUGIA",   "Perugia",   "Umbria",                43.11, 12.39, 493, 24.0, 35},
    {"POTENZA",   "Potenza",   "Basilicata",            40.64, 15.80, 819, 22.0, 25},
    {"ROMA",      "Roma",      "Lazio",                41.90, 12.50,  21, 25.5, 20},
    {"TORINO",    "Torino",    "Piemonte",             45.07,  7.67, 239, 23.2, 56},
    {"TRENTO",    "Trento",    "Trentino-Alto Adige",   46.07, 11.12, 190, 22.5, 70},
    {"TRIESTE",   "TRIESTE",   "Friuli-Venezia Giulia",  45.65, 13.77,   2, 24.5, 65},
    {"VENEZIA",   "Venezia",   "Veneto",               45.44, 12.32,   1, 24.5, 50},
};
#define N_METADATA ((int)(sizeof(METADATA) / sizeof(METADATA[0])))

/* Ciudades cargadas desde clima.conf */
typedef struct {
    char key[64];
    char region_s[64];   /* región leída de clima.conf (o vacío)      */
    const char *name;    /* = key si no hay metadata                  */
    const char *region;  /* apunta a region_s o a METADATA            */
    double lat;
    double lon;
    int    alt;
    double t_mean;
    int    precip;
    int    es_default;   /* 1 si la línea del config está sin '#'     */
} Ciudad;

static Ciudad  g_cities[MAX_CITIES];
static int     g_n_cities = 0;
static int     g_hay_config = 0;

/* ------------------------------------------------------------------ */
/* Estructuras de datos                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    char date[11];
    int  hour;
    char iso[24];
    double temp;
    double humid;
    double precip_prob;
    double precip;
    double pressure;
    const char *condition;
    int  raw_code;
} Hourly;

typedef struct {
    char date[11];
    double t_max;
    double t_min;
    double precip_sum;
    char dow[16];
} ForecastDay;

typedef struct {
    double t_min, t_max, t_mean;
    int    t_min_hour, t_max_hour;
    double h_min, h_max;
    int    h_min_hour, h_max_hour;
    double precip;
    const char *condition;
    const char *dominant;
} DailyStats;

typedef struct {
    int n_days;
    double mean_obs_max, mean_obs_min, mean_obs;
    double mean_normal;
    double mean_anomaly;
    int dry_streak;
    double total_precip;
    const char *trend;
    int heatwave;
} HistResult;

typedef struct {
    double daily_risks[MAX_DAYS];
    int n;
    double avg_risk;
    const char *trend;
    double peak_risk;
    int peak_day;
} MosquitoRisk;

typedef struct {
    int has_storm;
    const char *severity;   /* NULL si no hay tormenta */
    int hours[24];
    int n_hours;
    int night_storm;
    double conv_precip;
} StormDay;

typedef struct {
    StormDay daily[MAX_DAYS];
    int n;
    int has_any_storm;
    int severe_days;
    int night_storm_days;
} StormResult;

/* Globales poblados por build_forecast() */
static Hourly      g_hourly[MAX_HOURLY];
static int         g_n_hourly = 0;
static ForecastDay g_fdays[MAX_DAYS];
static int         g_n_fdays = 0;
static DailyStats  g_dstats[MAX_DAYS];
static int         g_n_stats = 0;

/* Histórico diario (fechas < hoy), poblado por build_forecast() */
static char    g_hist_dates[MAX_DAYS][16];
static double  g_hist_min[MAX_DAYS];
static double  g_hist_max[MAX_DAYS];
static double  g_hist_precip[MAX_DAYS];
static int     g_n_hist = 0;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */
static void print_header(const char *title) {
    int i;
    printf("\n");
    for (i = 0; i < 78; i++) printf("=");
    printf("\n  %s\n", title);
    for (i = 0; i < 78; i++) printf("=");
    printf("\n");
}

/* Ancho visual (columnas de terminal) de una cadena UTF-8:
   1 byte  = 1 col (ASCII)
   2 bytes = 1 col (acentos: ó, é, °)
   3 bytes = 1 col (símbolos: ☀, ─, ⚠, ➡)
   4 bytes = 2 cols (emoji: 🌤, ⛈, 🔥)                     */
static size_t disp_width(const char *s) {
    size_t w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p < 0x80) { w++; p++; }
        else if ((*p & 0xE0) == 0xC0) { w++; p += 2; }
        else if ((*p & 0xF0) == 0xE0) { w++; p += 3; }
        else if ((*p & 0xF8) == 0xF0) { w += 2; p += 4; }
        else { w++; p++; }
    }
    return w;
}

/* Alinear a la izquierda en "width" columnas visuales (pad a la derecha). */
static void pad_left(char *buf, size_t n, const char *s, int width) {
    size_t w = disp_width(s);
    int pad = width - (int)w;
    if (pad < 0) pad = 0;
    if (buf != s) {
        snprintf(buf, n, "%s%*s", s, pad, "");
        return;
    }
    size_t len = strlen(s);
    if (len >= n) len = n - 1;
    memmove(buf, s, len);
    buf[len] = '\0';
    size_t avail = n - 1 - len;
    int add = pad < (int)avail ? pad : (int)avail;
    memset(buf + len, ' ', (size_t)add);
    buf[len + (size_t)add] = '\0';
}

static void date_iso_offset(int offset, char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    tm.tm_mday += offset;
    time_t t2 = mktime(&tm);
    struct tm *res = localtime(&t2);
    strftime(buf, n, "%Y-%m-%d", res);
}

static void today_parts(int *y, int *m, int *d) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    *y = tm->tm_year + 1900;
    *m = tm->tm_mon + 1;
    *d = tm->tm_mday;
}

static void today_iso(char *buf, size_t n) {
    date_iso_offset(0, buf, n);
}

/* "%A" en inglés (igual que Python) */
static void dow_name(const char *date, char *buf, size_t n) {
    int y, m, d;
    struct tm t;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    mktime(&t);
    strftime(buf, n, "%A", &t);
}

static const char *dow_short(const char *dow) {
    if (!strcmp(dow, "Monday"))    return "Lun";
    if (!strcmp(dow, "Tuesday"))   return "Mar";
    if (!strcmp(dow, "Wednesday")) return "Mié";
    if (!strcmp(dow, "Thursday"))  return "Jue";
    if (!strcmp(dow, "Friday"))    return "Vie";
    if (!strcmp(dow, "Saturday"))  return "Sáb";
    if (!strcmp(dow, "Sunday"))    return "Dom";
    return dow;
}

static const char *dow_short_upper(const char *dow) {
    if (!strcmp(dow, "Monday"))    return "LUN";
    if (!strcmp(dow, "Tuesday"))   return "MAR";
    if (!strcmp(dow, "Wednesday")) return "MIÉ";
    if (!strcmp(dow, "Thursday"))  return "JUE";
    if (!strcmp(dow, "Friday"))    return "VIE";
    if (!strcmp(dow, "Saturday"))  return "SÁB";
    if (!strcmp(dow, "Sunday"))    return "DOM";
    return dow;
}

/* mes corto estilo "Jul" (vía strftime) */
static void month_short(const char *date, char *buf, size_t n) {
    int y, m, d;
    struct tm t;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    mktime(&t);
    strftime(buf, n, "%b", &t);
}

/* Devuelve la ruta de clima.conf: el archivo en el directorio actual, o si
 * no existe, el archivo junto al ejecutable. NULL si no hay ninguno. */
static const char *config_path(void) {
    static char junto_bin[1024];

#ifdef _WIN32
    junto_bin[0] = '\0';
    GetModuleFileNameA(NULL, junto_bin, (DWORD)sizeof(junto_bin) - 1);
    {
        char *last = NULL, *p;
        for (p = junto_bin; *p; p++)
            if (*p == '\\' || *p == '/') last = p;
        if (last) {
            size_t pos = (size_t)(last - junto_bin + 1);
            snprintf(junto_bin + pos, sizeof(junto_bin) - pos, "%s", CONFIG_FILE);
        } else {
            junto_bin[0] = '\0';
        }
    }
#else
    junto_bin[0] = '\0';
    {
        char tmp[1024];
        ssize_t n = readlink("/proc/self/exe", tmp, sizeof(tmp) - 1);
        if (n >= 0) {
            tmp[n] = '\0';
            char *slash = strrchr(tmp, '/');
            if (slash) {
                size_t pos = (size_t)(slash - tmp + 1);
                snprintf(junto_bin, sizeof(junto_bin), "%.*s%s",
                         (int)pos, tmp, CONFIG_FILE);
            }
        }
    }
#endif

    {
        FILE *chk = fopen(CONFIG_FILE, "r");
        if (chk) { fclose(chk); return CONFIG_FILE; }
    }
    if (junto_bin[0]) {
        FILE *chk = fopen(junto_bin, "r");
        if (chk) { fclose(chk); return junto_bin; }
    }
    return NULL;
}

/* Localiza una ciudad cargada (por clave, no distingue may/min). */
static const Ciudad *find_ciudad(const char *key) {
    int i;
    for (i = 0; i < g_n_cities; i++) {
        if (!strcasecmp(g_cities[i].key, key))
            return &g_cities[i];
    }
    return NULL;
}

/* Recorre la tabla de metadata buscando la clave; devuelve puntero o NULL. */
static const MetaCiudad *find_meta(const char *key) {
    int i;
    for (i = 0; i < N_METADATA; i++) {
        if (!strcasecmp(METADATA[i].key, key))
            return &METADATA[i];
    }
    return NULL;
}

/* Carga las ciudades desde clima.conf. Formato por línea:
 *   [ #]  NOMBRE;lat,lon[;REGION]
 * La línea SIN '#' (sin comentar) es la ciudad por defecto (es_default=1).
 * Si NOMBRE coincide con una capital conocida, se completa nombre,
 * altitud y normales; si no, se usan solo las coordenadas y la región
 * (si se indica) del propio archivo.
 * Devuelve la clave por defecto (o NULL). */
static const char *cargar_config(void) {
    const char *path = config_path();
    const char *defecto = NULL;
    char buf[CONF_LINE];
    FILE *f;
    int i = 0;

    if (!path) return NULL;                  /* no hay config            */
    g_hay_config = 1;

    f = fopen(path, "r");
    if (!f) return NULL;

    while (fgets(buf, sizeof(buf), f) && i < MAX_CITIES) {
        char *s = buf, *semi, *coord, *reg = NULL;
        double lat = 0, lon = 0;
        int es_def = 1;

        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#') { es_def = 0; s++; }
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\n' || *s == '\0') continue;

        semi = strchr(s, ';');
        if (semi) {
            *semi = '\0';
            coord = semi + 1;
            reg = strchr(coord, ';');
            if (reg) *reg = '\0';
            while (*coord == ' ' || *coord == '\t') coord++;
            lat = atof(coord);
            while (*coord && *coord != ',' &&
                   *coord != ' ' && *coord != '\t') coord++;
            if (*coord == ',') coord++;
            while (*coord == ' ' || *coord == '\t') coord++;
            lon = atof(coord);
        }
        /* sin ';' -> solo el nombre, coords las tomará de la metadata */

        /* trim blanco a la derecha del nombre */
        {
            size_t len = strlen(s);
            while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r'))
                s[--len] = '\0';
        }

        /* normalizar a mayúsculas */
        for (int k = 0; s[k]; k++)
            if (s[k] >= 'a' && s[k] <= 'z') s[k] = s[k] - 'a' + 'A';

        if (s[0] == '\0') continue;

        const MetaCiudad *m = find_meta(s);

        /* Se acepta si coincide con una capital conocida (con o sin coords),
         * o si el nombre trae coordenadas válidas (>0). Se descartan líneas
         * de texto de cabecera (que llevan ':' como "por ejemplo: ..."). */
        if (!m) {
            if (lat == 0 && lon == 0) continue;
            if (strchr(s, ':')) continue;          /* "por ejemplo: ..." */
        }

        snprintf(g_cities[i].key, sizeof(g_cities[i].key), "%s", s);
        g_cities[i].lat = semi && lat ? lat : (m ? m->lat : 0);
        g_cities[i].lon = semi && lon ? lon : (m ? m->lon : 0);
        g_cities[i].es_default = es_def;
        g_cities[i].alt     = 0;
        g_cities[i].t_mean  = 0;
        g_cities[i].precip  = 0;
        g_cities[i].name    = g_cities[i].key;
        g_cities[i].region_s[0] = '\0';
        g_cities[i].region  = g_cities[i].region_s;

        if (reg) {
            char *r = reg + 1;
            while (*r == ' ' || *r == '\t') r++;
            size_t rlen = strlen(r);
            while (rlen > 0 && (r[rlen-1] == '\n' || r[rlen-1] == '\r' ||
                                r[rlen-1] == ' ' || r[rlen-1] == '\t'))
                r[--rlen] = '\0';
            if (r[0])
                snprintf(g_cities[i].region_s, sizeof(g_cities[i].region_s), "%s", r);
        }

        if (m) {
            g_cities[i].name   = m->name;
            g_cities[i].alt    = m->alt;
            g_cities[i].t_mean = m->t_mean;
            g_cities[i].precip = m->precip;
            if (!g_cities[i].region_s[0] && m->region)
                snprintf(g_cities[i].region_s, sizeof(g_cities[i].region_s), "%s", m->region);
        }

        if (es_def && !defecto) defecto = g_cities[i].key;
        i++;
    }

    g_n_cities = i;
    fclose(f);
    return defecto;
}

/* Carga de emergencia si no hay clima.conf: las 20 capitales (TORINO defecto). */
static const char *cargar_fallback(void) {
    int i;
    g_hay_config = 0;
    for (i = 0; i < N_METADATA && i < MAX_CITIES; i++) {
        snprintf(g_cities[i].key, sizeof(g_cities[i].key), "%s", METADATA[i].key);
        g_cities[i].name   = METADATA[i].name;
        g_cities[i].region = METADATA[i].region;
        g_cities[i].lat    = METADATA[i].lat;
        g_cities[i].lon    = METADATA[i].lon;
        g_cities[i].alt    = METADATA[i].alt;
        g_cities[i].t_mean = METADATA[i].t_mean;
        g_cities[i].precip = METADATA[i].precip;
        g_cities[i].es_default = !strcasecmp(METADATA[i].key, "TORINO");
    }
    g_n_cities = i;
    return "TORINO";
}

/* "Torino (Piemonte, 45.07°N, 7.69°E)" — ciudad, región y coordenadas juntas */
static void ciudad_label(const Ciudad *c, char *buf, size_t n) {
    if (c->region && c->region[0])
        snprintf(buf, n, "%s (%s, %.2f°%c, %.2f°%c)",
                 c->name, c->region,
                 fabs(c->lat), c->lat > 0 ? 'N' : 'S',
                 fabs(c->lon), c->lon > 0 ? 'E' : 'O');
    else
        snprintf(buf, n, "%s (%.2f°%c, %.2f°%c)",
                 c->name,
                 fabs(c->lat), c->lat > 0 ? 'N' : 'S',
                 fabs(c->lon), c->lon > 0 ? 'E' : 'O');
}

/* ------------------------------------------------------------------ */
/* HTTP fetch: libcurl (Linux/macOS) o WinHTTP (Windows)               */
/* ------------------------------------------------------------------ */
typedef struct {
    char *data;
    size_t size;
} MemBuf;

static void membuf_append(MemBuf *mem, const char *ptr, size_t len) {
    char *tmp = realloc(mem->data, mem->size + len + 1);
    if (!tmp) return;
    mem->data = tmp;
    memcpy(mem->data + mem->size, ptr, len);
    mem->size += len;
    mem->data[mem->size] = '\0';
}

#ifndef _WIN32
static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    membuf_append((MemBuf *)userdata, (const char *)ptr, size * nmemb);
    return size * nmemb;
}
#endif

static void openmeteo_url(const Ciudad *c, int days_forecast,
                          int days_history, char *url, size_t n) {
    char start[16], end[16];
    date_iso_offset(-days_history, start, sizeof(start));
    date_iso_offset(days_forecast - 1, end, sizeof(end));

    snprintf(url, n,
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%.2f&longitude=%.2f"
        "&timezone=Europe/Rome"
        "&start_date=%s&end_date=%s"
        "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max"
        "&hourly=temperature_2m,relative_humidity_2m,precipitation_probability,precipitation,weather_code,surface_pressure"
        "&models=best_match",
        c->lat, c->lon, start, end);
}

#ifndef _WIN32
static cJSON *fetch_openmeteo(const Ciudad *c, int days_forecast, int days_history) {
    char url[URL_LEN];
    CURL *curl;
    CURLcode res;
    MemBuf mem = {NULL, 0};
    cJSON *json;

    openmeteo_url(c, days_forecast, days_history, url, sizeof(url));

    curl = curl_easy_init();
    if (!curl) return NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "  ERROR fetching data: %s\n", curl_easy_strerror(res));
        free(mem.data);
        return NULL;
    }
    json = cJSON_Parse(mem.data);
    free(mem.data);
    return json;
}
#else /* _WIN32 */
static cJSON *fetch_openmeteo(const Ciudad *c, int days_forecast, int days_history) {
    char url[URL_LEN];
    char host[128], path[URL_LEN];
    wchar_t whost[128], wpath[URL_LEN];
    DWORD dwErr = 0;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    MemBuf mem = {NULL, 0};
    cJSON *json = NULL;

    openmeteo_url(c, days_forecast, days_history, url, sizeof(url));

    {
        const char *p = strstr(url, "://");
        const char *slash;
        p = p ? p + 3 : url;
        slash = strchr(p, '/');
        if (slash) {
            snprintf(host, sizeof(host), "%.*s", (int)(slash - p), p);
            snprintf(path, sizeof(path), "%s", slash);
        } else {
            snprintf(host, sizeof(host), "%s", p);
            snprintf(path, sizeof(path), "/");
        }
    }

    MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, sizeof(whost) / sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, sizeof(wpath) / sizeof(wchar_t));

    hSession = WinHttpOpen(L"Analisis-de-Clima/2.1",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { dwErr = GetLastError(); goto done; }

    hConnect = WinHttpConnect(hSession, whost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { dwErr = GetLastError(); goto done; }

    hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath, NULL,
                                  WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE);
    if (!hRequest) { dwErr = GetLastError(); goto done; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        dwErr = GetLastError();
        goto done;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { dwErr = GetLastError(); goto done; }

    for (;;) {
        DWORD avail = 0;
        char chunk[8192];
        DWORD read = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) { dwErr = GetLastError(); break; }
        if (avail == 0) break;
        if (avail > sizeof(chunk)) avail = sizeof(chunk);
        if (!WinHttpReadData(hRequest, chunk, avail, &read)) { dwErr = GetLastError(); break; }
        if (read == 0) break;
        membuf_append(&mem, chunk, read);
    }

done:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    if (mem.size == 0) {
        fprintf(stderr, "  ERROR fetching data (WinHTTP error: %lu)\n", (unsigned long)dwErr);
        free(mem.data);
        return NULL;
    }
    json = cJSON_Parse(mem.data);
    free(mem.data);
    return json;
}
#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/* Códigos meteorológicos WMO -> condición                             */
/* ------------------------------------------------------------------ */
static const char *weather_code_to_condition(int code) {
    if (code == -1 || code == 0)        return "Despejado";
    if (code == 1)                      return "Mayormente despejado";
    if (code == 2)                      return "Parcialmente nublado";
    if (code == 3)                      return "Nublado";
    if (code >= 4 && code <= 19)        return "Niebla";
    if (code >= 20 && code <= 29)       return "Lluvia ligera";
    if (code >= 30 && code <= 39)       return "Tormenta";
    if (code >= 40 && code <= 49)       return "Niebla densa";
    if (code >= 50 && code <= 59)       return "Lluvia ligera";
    if (code >= 60 && code <= 69)       return "Lluvia";
    if (code >= 70 && code <= 79)       return "Nieve";
    if (code >= 80 && code <= 84)       return "Lluvia";
    if (code >= 85 && code <= 86)       return "Nieve";
    if (code >= 90 && code <= 99)       return "Tormenta";
    return "Otros";
}

static const char *condition_icon(const char *cond) {
    if (strstr(cond, "Mayormente despejado")) return "🌤";
    if (strstr(cond, "Despejado"))            return "☀";
    if (strstr(cond, "Parcialmente nublado")) return "⛅";
    if (strstr(cond, "Nublado"))              return "☁";
    if (strstr(cond, "Niebla"))               return "🌫";
    if (strstr(cond, "Lluvia ligera"))        return "🌦";
    if (strstr(cond, "Lluvia"))               return "🌧";
    if (strstr(cond, "Tormenta"))             return "⛈";
    if (strstr(cond, "Nieve"))                return "❄";
    return "❓";
}

/* ------------------------------------------------------------------ */
/* Listar ciudades                                                     */
/* ------------------------------------------------------------------ */
static void list_ciudades(void) {
    int i;
    const Ciudad *c;
    printf("\n");
    printf("  Ciudades disponibles (%d) — desde %s:\n", g_n_cities, CONFIG_FILE);
    {
        char h1[96], h2[64], h3[32], h4[32], h5[32], h6[32];
        pad_left(h1, sizeof(h1), "Ciudad", 14);
        pad_left(h2, sizeof(h2), "Lat", 8);
        pad_left(h3, sizeof(h3), "Lon", 8);
        pad_left(h4, sizeof(h4), "Región", 22);
        pad_left(h5, sizeof(h5), "Alt", 5);
        pad_left(h6, sizeof(h6), "Normas jul", 12);
        printf("  %s %s %s %s %s %s  Def.\n", h1, h2, h3, h4, h5, h6);
        pad_left(h1, sizeof(h1), "──────────────", 14);
        pad_left(h2, sizeof(h2), "────────", 8);
        pad_left(h3, sizeof(h3), "────────", 8);
        pad_left(h4, sizeof(h4), "─────────────────────", 22);
        pad_left(h5, sizeof(h5), "─────", 5);
        pad_left(h6, sizeof(h6), "──────────", 12);
        printf("  %s %s %s %s %s %s  ──\n", h1, h2, h3, h4, h5, h6);
    }
for (i = 0; i < g_n_cities; i++) {
        c = &g_cities[i];
        printf("  %-14s %6.2f°%c  %6.2f°%c  %-22s %5dm  %7.1f°C %s\n",
               c->name,
               fabs(c->lat), c->lat >= 0 ? 'N' : 'S',
               fabs(c->lon), c->lon >= 0 ? 'E' : 'O',
               c->region ? c->region : "",
               c->alt,
               c->t_mean,
               c->es_default ? "<- por defecto" : "");
    }
    printf("\n  Usar: --ciudad CIUDAD (ej: --ciudad ROMA, --ciudad FIRENZE)\n");
    printf("  Las ciudades se leen de %s. Añade nuevas con 'NOMBRE;lat,lon;REGION'.\n\n", CONFIG_FILE);
}

/* ------------------------------------------------------------------ */
/* Acceso a arrays JSON                                                */
/* ------------------------------------------------------------------ */
static double arr_num(cJSON *arr, int i) {
    cJSON *v;
    if (!cJSON_IsArray(arr)) return 0;
    if (i < 0 || i >= cJSON_GetArraySize(arr)) return 0;
    v = cJSON_GetArrayItem(arr, i);
    if (cJSON_IsNull(v)) return 0;
    if (cJSON_IsNumber(v)) return v->valuedouble;
    return 0;
}

static const char *arr_str(cJSON *arr, int i) {
    cJSON *v;
    if (!cJSON_IsArray(arr)) return "";
    if (i < 0 || i >= cJSON_GetArraySize(arr)) return "";
    v = cJSON_GetArrayItem(arr, i);
    if (cJSON_IsString(v)) return v->valuestring;
    return "";
}

/* ------------------------------------------------------------------ */
/* Análisis histórico                                                  */
/* ------------------------------------------------------------------ */
static HistResult analyze_historical(cJSON *raw, const Ciudad *c) {
    HistResult h;
    cJSON *daily, *dates;
    char today[16];
    int i, n, cnt;
    double mean_obs_max = 0, mean_obs_min = 0, mean_obs;
    double total_precip = 0;
    int dry_streak = 0;

    memset(&h, 0, sizeof(h));
    h.trend = "stable";

    daily = cJSON_GetObjectItem(raw, "daily");
    dates = daily ? cJSON_GetObjectItem(daily, "time") : NULL;
    n = dates ? cJSON_GetArraySize(dates) : 0;
    today_iso(today, sizeof(today));

    g_n_hist = 0;
    for (i = 0; i < n && g_n_hist < MAX_DAYS; i++) {
        const char *d = arr_str(dates, i);
        if (strcmp(d, today) < 0) {
            snprintf(g_hist_dates[g_n_hist], sizeof(g_hist_dates[g_n_hist]), "%s", d);
            g_hist_min[g_n_hist]   = arr_num(cJSON_GetObjectItem(daily, "temperature_2m_min"), i);
            g_hist_max[g_n_hist]   = arr_num(cJSON_GetObjectItem(daily, "temperature_2m_max"), i);
            g_hist_precip[g_n_hist] = arr_num(cJSON_GetObjectItem(daily, "precipitation_sum"), i);
            g_n_hist++;
        }
    }

    cnt = g_n_hist;
    if (cnt == 0) return h;

    for (i = 0; i < cnt; i++) {
        mean_obs_max += g_hist_max[i];
        mean_obs_min += g_hist_min[i];
        total_precip += g_hist_precip[i];
    }
    mean_obs_max /= cnt;
    mean_obs_min /= cnt;
    mean_obs = (mean_obs_max + mean_obs_min) / 2.0;

    for (i = cnt - 1; i >= 0; i--) {
        if (g_hist_precip[i] == 0) dry_streak++;
        else break;
    }

    h.n_days = cnt;
    h.mean_obs_max = mean_obs_max;
    h.mean_obs_min = mean_obs_min;
    h.mean_obs = mean_obs;
    h.mean_normal = c->t_mean;
    h.mean_anomaly = mean_obs - c->t_mean;
    h.total_precip = total_precip;
    h.dry_streak = dry_streak;

    if (cnt >= 4) {
        double avg_recent = (g_hist_max[cnt-1] + g_hist_max[cnt-2] + g_hist_max[cnt-3]) / 3.0;
        double avg_prior  = (g_hist_max[cnt-2] + g_hist_max[cnt-3] + g_hist_max[cnt-4]) / 3.0;
        double diff = avg_recent - avg_prior;
        if (diff > 1.5)       h.trend = "rapid_warming";
        else if (diff > 0.5)  h.trend = "warming";
        else if (diff < -1.5) h.trend = "rapid_cooling";
        else if (diff < -0.5) h.trend = "cooling";
        else                  h.trend = "stable";
    }

    h.heatwave = (dry_streak >= 3 && h.mean_anomaly > 3.0);
    return h;
}

/* ------------------------------------------------------------------ */
/* Construcción del pronóstico (globals)                               */
/* ------------------------------------------------------------------ */
static void build_forecast(cJSON *raw) {
    cJSON *hourly, *daily, *arr;
    int i;
    int h_count, d_count;
    char today[16];

    today_iso(today, sizeof(today));
    g_n_hourly = 0;
    g_n_fdays = 0;

    hourly = cJSON_GetObjectItem(raw, "hourly");
    daily  = cJSON_GetObjectItem(raw, "daily");

    arr = cJSON_GetObjectItem(hourly, "time");
    h_count = arr ? cJSON_GetArraySize(arr) : 0;
    if (h_count > MAX_HOURLY) h_count = MAX_HOURLY;

    for (i = 0; i < h_count; i++) {
        Hourly *h = &g_hourly[i];
        const char *t = arr_str(arr, i);
        int y, mo, dd, hh, mi;
        memset(h, 0, sizeof(*h));
        if (sscanf(t, "%d-%d-%dT%d:%d", &y, &mo, &dd, &hh, &mi) == 5) {
            snprintf(h->date, sizeof(h->date), "%04d-%02d-%02d", y, mo, dd);
            h->hour = hh;
            snprintf(h->iso, sizeof(h->iso), "%04d-%02d-%02dT%02d:%02d+02:00", y, mo, dd, hh, mi);
        }
        h->temp        = arr_num(cJSON_GetObjectItem(hourly, "temperature_2m"), i);
        h->humid       = arr_num(cJSON_GetObjectItem(hourly, "relative_humidity_2m"), i);
        h->precip_prob = arr_num(cJSON_GetObjectItem(hourly, "precipitation_probability"), i);
        h->precip      = arr_num(cJSON_GetObjectItem(hourly, "precipitation"), i);
        h->pressure    = arr_num(cJSON_GetObjectItem(hourly, "surface_pressure"), i);
        h->raw_code    = (int)arr_num(cJSON_GetObjectItem(hourly, "weather_code"), i);
        h->condition   = weather_code_to_condition(h->raw_code);
    }
    g_n_hourly = h_count;

    arr = cJSON_GetObjectItem(daily, "time");
    d_count = arr ? cJSON_GetArraySize(arr) : 0;

    for (i = 0; i < d_count; i++) {
        ForecastDay *fd;
        const char *d = arr_str(arr, i);
        if (strcmp(d, today) < 0) continue;
        if (g_n_fdays >= MAX_DAYS) break;
        fd = &g_fdays[g_n_fdays];
        snprintf(fd->date, sizeof(fd->date), "%s", d);
        fd->t_max = arr_num(cJSON_GetObjectItem(daily, "temperature_2m_max"), i);
        fd->t_min = arr_num(cJSON_GetObjectItem(daily, "temperature_2m_min"), i);
        fd->precip_sum = arr_num(cJSON_GetObjectItem(daily, "precipitation_sum"), i);
        dow_name(d, fd->dow, sizeof(fd->dow));
        g_n_fdays++;
    }
}

/* ------------------------------------------------------------------ */
/* Estadísticas diarias                                                */
/* ------------------------------------------------------------------ */
static const char *compute_cond_summary(const char *date_str, const char *dominant) {
    int i;
    int any_tormenta = 0, any_lluvia = 0, any_niebla = 0;
    int any_nublado = 0, any_parc = 0, sunny = 0;
    for (i = 0; i < g_n_hourly; i++) {
        const Hourly *h = &g_hourly[i];
        if (strcmp(h->date, date_str) != 0) continue;
        if (strstr(h->condition, "Tormenta"))    any_tormenta = 1;
        if (strstr(h->condition, "Lluvia"))      any_lluvia = 1;
        if (strstr(h->condition, "Niebla"))      any_niebla = 1;
        if (strstr(h->condition, "Nublado"))     any_nublado = 1;
        if (strstr(h->condition, "Parcialmente")) any_parc = 1;
        if (strstr(h->condition, "Soleado") || strstr(h->condition, "Despejado")) sunny++;
    }
    if (any_tormenta) return "Tormentas aisladas";
    if (any_lluvia)   return "Lluvias, parcialmente nublado";
    if (any_niebla)   return "Niebla, nubosidad";
    if (any_nublado && any_parc) return "Nubosidad variable";
    if (sunny >= 18)  return "Soleado, despejado";
    if (any_nublado)  return "Mayormente nublado";
    return dominant;
}

static int compute_daily_stats(const char *date_str, DailyStats *out) {
    int i;
    double t_min, t_max, t_mean, h_min, h_max, total_precip;
    int n_t = 0;
    int t_min_hour = -1, t_max_hour = -1, h_min_hour = 0, h_max_hour = 0;
    int cnt_despejado = 0, cnt_parc = 0, cnt_nublado = 0, cnt_lluvia = 0,
        cnt_tormenta = 0, cnt_niebla = 0, cnt_lligera = 0, cnt_otro = 0;
    int dom_max = -1;
    const char *dominant = "Despejado";

    memset(out, 0, sizeof(*out));

    t_min = 1e9; t_max = -1e9;
    h_min = 1e9; h_max = -1e9;
    t_mean = 0; total_precip = 0;

    for (i = 0; i < g_n_hourly; i++) {
        const Hourly *h = &g_hourly[i];
        if (strcmp(h->date, date_str) != 0) continue;
        if (h->temp < t_min) { t_min = h->temp; t_min_hour = h->hour; }
        if (h->temp > t_max) { t_max = h->temp; t_max_hour = h->hour; }
        t_mean += h->temp;
        n_t++;
        if (h->humid < h_min) { h_min = h->humid; h_min_hour = h->hour; }
        if (h->humid > h_max) { h_max = h->humid; h_max_hour = h->hour; }
        total_precip += h->precip;

        if (!strcmp(h->condition, "Despejado")) cnt_despejado++;
        else if (!strcmp(h->condition, "Parcialmente nublado")) cnt_parc++;
        else if (!strcmp(h->condition, "Nublado")) cnt_nublado++;
        else if (!strcmp(h->condition, "Lluvia ligera")) cnt_lligera++;
        else if (!strcmp(h->condition, "Lluvia")) cnt_lluvia++;
        else if (!strcmp(h->condition, "Tormenta")) cnt_tormenta++;
        else if (strstr(h->condition, "Niebla")) cnt_niebla++;
        else cnt_otro++;
    }
    if (n_t == 0) return 0;

    if (cnt_tormenta > dom_max) { dom_max = cnt_tormenta; dominant = "Tormenta"; }
    if (cnt_lluvia  > dom_max)  { dom_max = cnt_lluvia;  dominant = "Lluvia"; }
    if (cnt_lligera > dom_max)  { dom_max = cnt_lligera; dominant = "Lluvia ligera"; }
    if (cnt_niebla  > dom_max)  { dom_max = cnt_niebla;  dominant = "Niebla"; }
    if (cnt_nublado > dom_max)  { dom_max = cnt_nublado; dominant = "Nublado"; }
    if (cnt_parc    > dom_max)  { dom_max = cnt_parc;    dominant = "Parcialmente nublado"; }
    if (cnt_despejado > dom_max){ dom_max = cnt_despejado; dominant = "Despejado"; }
    (void)cnt_otro;

    out->t_min = t_min; out->t_max = t_max;
    out->t_mean = t_mean / n_t;
    out->t_min_hour = t_min_hour;
    out->t_max_hour = t_max_hour;
    out->h_min = h_min; out->h_max = h_max;
    out->h_min_hour = h_min_hour; out->h_max_hour = h_max_hour;
    out->precip = total_precip;
    out->dominant = dominant;
    out->condition = compute_cond_summary(date_str, dominant);

    return 1;
}

/* ------------------------------------------------------------------ */
/* Riesgo de tormentas convectivas                                     */
/* ------------------------------------------------------------------ */
static StormResult analyze_convective_risk(void) {
    StormResult sr;
    int d, i;
    memset(&sr, 0, sizeof(sr));
    sr.n = g_n_fdays;

    for (d = 0; d < g_n_fdays; d++) {
        StormDay *sd = &sr.daily[d];
        const ForecastDay *fd = &g_fdays[d];
        int max_sev = 0;
        for (i = 0; i < g_n_hourly; i++) {
            const Hourly *h = &g_hourly[i];
            int code = h->raw_code;
            int sev = 0;
            if (strcmp(h->date, fd->date) != 0) continue;
            if (code == 95) sev = 95;
            else if (code == 96) sev = 96;
            else if (code == 97) sev = 97;
            else if (code == 99) sev = 99;
            if (sev) {
                if (sev > max_sev) max_sev = sev;
                if (sd->n_hours < 24) sd->hours[sd->n_hours++] = h->hour;
                sd->conv_precip += h->precip;
            }
        }
        if (max_sev >= 99)      sd->severity = "severa";
        else if (max_sev >= 96) sd->severity = "fuerte";
        else if (max_sev >= 95) sd->severity = "moderada";
        else if (max_sev >= 90) sd->severity = "leve";
        else                    sd->severity = NULL;

        sd->has_storm = sd->severity != NULL;
        for (i = 0; i < sd->n_hours; i++) {
            if (sd->hours[i] >= 0 && sd->hours[i] <= 5) { sd->night_storm = 1; break; }
        }
    }

    for (d = 0; d < sr.n; d++) {
        if (sr.daily[d].has_storm) sr.has_any_storm = 1;
        if (sr.daily[d].severity &&
            (!strcmp(sr.daily[d].severity, "fuerte") || !strcmp(sr.daily[d].severity, "severa")))
            sr.severe_days++;
        if (sr.daily[d].night_storm) sr.night_storm_days++;
    }
    return sr;
}

/* ------------------------------------------------------------------ */
/* Riesgo de zancudos                                                  */
/* ------------------------------------------------------------------ */
static const char *mosquito_level(double risk) {
    if (risk >= 7) return "ALTO";
    if (risk >= 4) return "MEDIO";
    if (risk >= 2) return "BAJO";
    return "MUY BAJO";
}

static MosquitoRisk compute_mosquito_risk(const HistResult *hist) {
    MosquitoRisk mr;
    int i, half;
    double avg, first = 0, second = 0, diff;
    int dry_boost = 0;

    memset(&mr, 0, sizeof(mr));
    mr.trend = "estable";
    mr.n = g_n_stats;

    for (i = 0; i < g_n_stats; i++) {
        const ForecastDay *fd = &g_fdays[i];
        const DailyStats *ds = &g_dstats[i];
        double t_mean = ds->t_mean;
        double h_mean = (ds->h_min + ds->h_max) / 2.0;
        double precip = fd->precip_sum;
        double t_score, h_score, p_score, risk;

        if (t_mean < 15)            t_score = 0.0;
        else if (t_mean < 25)       t_score = (t_mean - 15) / 10.0;
        else if (t_mean <= 30)      t_score = 1.0;
        else if (t_mean < 38)       t_score = 1 - (t_mean - 30) / 8.0;
        else                        t_score = 0.0;

        h_score = fmin(h_mean / 80.0, 1.0);
        p_score = precip > 0 ? fmin(precip / 5.0, 1.0) : 0.0;

        risk = (t_score * 0.45 + h_score * 0.35 + p_score * 0.20) * 10.0;
        if (risk > 10) risk = 10;
        mr.daily_risks[i] = round(risk * 10.0) / 10.0;
    }

    avg = 0;
    for (i = 0; i < mr.n; i++) avg += mr.daily_risks[i];
    if (mr.n > 0) avg /= mr.n;

    if (mr.n >= 4) {
        half = mr.n / 2;
        for (i = 0; i < half; i++) first += mr.daily_risks[i];
        for (i = half; i < mr.n; i++) second += mr.daily_risks[i];
        first  /= half;
        second /= mr.n - half;
        diff = second - first;
        if (diff > 1.0)       mr.trend = "aumentando";
        else if (diff < -1.0) mr.trend = "disminuyendo";
        else                  mr.trend = "estable";
    }

    if (hist->dry_streak >= 2 && hist->total_precip > 0) dry_boost = 1;
    mr.avg_risk = avg + dry_boost;
    if (mr.avg_risk > 10) mr.avg_risk = 10;
    mr.avg_risk = round(mr.avg_risk * 10.0) / 10.0;

    mr.peak_risk = 0; mr.peak_day = 0;
    for (i = 0; i < mr.n; i++) {
        if (mr.daily_risks[i] > mr.peak_risk) {
            mr.peak_risk = mr.daily_risks[i];
            mr.peak_day = i;
        }
    }
    return mr;
}

/* ------------------------------------------------------------------ */
/* Reporte "hoy"                                                       */
/* ------------------------------------------------------------------ */
static void today_report(const Ciudad *c) {
    cJSON *raw;
    HistResult hist;
    char today[16], label[128], dow[16], mon[8];
    int y, m, d, now_h, i;
    double t_min = 1e9, t_max = -1e9, t_mean = 0, t_now = 0, h_now = 0, p_now = 0;
    double h_min = 1e9, h_max = -1e9, h_mean = 0;
    double p_min = 1e9, p_max = -1e9, p_mean = 0;
    int n_t = 0, n_p = 0, found = 0;
    int t_min_h = 0, t_max_h = 0, h_min_h = 0, h_max_h = 0, p_min_h = 0, p_max_h = 0;
    int any_rain = 0, night_storm = 0, day_storm = 0;
    int cnt_despejado = 0, cnt_parc = 0, cnt_nublado = 0, cnt_lluvia = 0,
        cnt_tormenta = 0, cnt_niebla = 0, cnt_lligera = 0;
    int dom_max = -1;
    const char *dominant = "Despejado";
    const char *temp_trend;
    double trend_slope = 0;

    raw = fetch_openmeteo(c, 3, HIST_DAYS);
    if (!raw) {
        fprintf(stderr, "  ERROR: No se pudieron obtener datos.\n");
        return;
    }
    hist = analyze_historical(raw, c);
    build_forecast(raw);
    cJSON_Delete(raw);

    today_iso(today, sizeof(today));
    {
        time_t tt = time(NULL);
        struct tm *tm = localtime(&tt);
        now_h = tm->tm_hour;
    }

    for (i = 0; i < g_n_hourly; i++) {
        const Hourly *h = &g_hourly[i];
        if (strcmp(h->date, today) != 0) continue;
        found = 1;
        if (h->temp < t_min) t_min = h->temp;
        if (h->temp > t_max) t_max = h->temp;
        t_mean += h->temp;
        n_t++;
        if (h->humid < h_min) h_min = h->humid;
        if (h->humid > h_max) h_max = h->humid;
        if (h->pressure != 0) {
            if (h->pressure < p_min) p_min = h->pressure;
            if (h->pressure > p_max) p_max = h->pressure;
            p_mean += h->pressure;
            n_p++;
        }
        if (h->precip > 0) any_rain = 1;
        if (strstr(h->condition, "Tormenta")) {
            if (h->hour <= 5) night_storm = 1; else day_storm = 1;
        }
        if (!strcmp(h->condition, "Despejado")) cnt_despejado++;
        else if (!strcmp(h->condition, "Parcialmente nublado")) cnt_parc++;
        else if (!strcmp(h->condition, "Nublado")) cnt_nublado++;
        else if (!strcmp(h->condition, "Lluvia ligera")) cnt_lligera++;
        else if (!strcmp(h->condition, "Lluvia")) cnt_lluvia++;
        else if (!strcmp(h->condition, "Tormenta")) cnt_tormenta++;
        else if (strstr(h->condition, "Niebla")) cnt_niebla++;
    }
    if (!found) {
        printf("  No hay datos para hoy.\n");
        return;
    }
    if (n_t > 0) t_mean /= n_t;
    h_mean = (h_min + h_max) / 2.0;
    if (n_p > 0) p_mean /= n_p;

    if (cnt_tormenta > dom_max) { dom_max = cnt_tormenta; dominant = "Tormenta"; }
    if (cnt_lluvia  > dom_max)  { dom_max = cnt_lluvia;  dominant = "Lluvia"; }
    if (cnt_lligera > dom_max)  { dom_max = cnt_lligera; dominant = "Lluvia ligera"; }
    if (cnt_niebla  > dom_max)  { dom_max = cnt_niebla;  dominant = "Niebla"; }
    if (cnt_nublado > dom_max)  { dom_max = cnt_nublado; dominant = "Nublado"; }
    if (cnt_parc    > dom_max)  { dom_max = cnt_parc;    dominant = "Parcialmente nublado"; }
    if (cnt_despejado > dom_max){ dom_max = cnt_despejado; dominant = "Despejado"; }

    /* ahora: buscar la hora actual */
    {
        int found_now = 0;
        for (i = 0; i < g_n_hourly; i++) {
            const Hourly *h = &g_hourly[i];
            if (strcmp(h->date, today) != 0) continue;
            if (h->hour == now_h) {
                t_now = h->temp; h_now = h->humid; p_now = h->pressure;
                found_now = 1;
                break;
            }
        }
        if (!found_now) {
            for (i = 0; i < g_n_hourly; i++) {
                const Hourly *h = &g_hourly[i];
                if (strcmp(h->date, today) != 0) continue;
                t_now = h->temp; h_now = h->humid; p_now = h->pressure;
                break;
            }
        }
    }

    for (i = 0; i < g_n_hourly; i++) {
        const Hourly *h = &g_hourly[i];
        if (strcmp(h->date, today) != 0) continue;
        if (h->temp == t_min) t_min_h = h->hour;
        if (h->temp == t_max) t_max_h = h->hour;
        if (h->humid == h_min) h_min_h = h->hour;
        if (h->humid == h_max) h_max_h = h->hour;
        if (h->pressure == p_min) p_min_h = h->hour;
        if (h->pressure == p_max) p_max_h = h->hour;
    }

    /* tendencia: primera vs última hora */
    {
        double first_temp = 0, last_temp = 0;
        int cnt = 0;
        for (i = 0; i < g_n_hourly; i++) {
            const Hourly *h = &g_hourly[i];
            if (strcmp(h->date, today) != 0) continue;
            first_temp = h->temp;
            break;
        }
        for (i = g_n_hourly - 1; i >= 0; i--) {
            const Hourly *h = &g_hourly[i];
            if (strcmp(h->date, today) != 0) continue;
            last_temp = h->temp;
            cnt++;
            break;
        }
        if (cnt > 1) trend_slope = (last_temp - first_temp) / cnt;
    }

    if (trend_slope > 1.5)       temp_trend = "CALENTAMIENTO RÁPIDO";
    else if (trend_slope > 0.5)  temp_trend = "CALENTAMIENTO";
    else if (trend_slope < -1.5) temp_trend = "ENFRIAMIENTO RÁPIDO";
    else if (trend_slope < -0.5) temp_trend = "ENFRIAMIENTO";
    else                         temp_trend = "ESTABLE";

    dow_name(today, dow, sizeof(dow));
    month_short(today, mon, sizeof(mon));
    today_parts(&y, &m, &d);
    ciudad_label(c, label, sizeof(label));

    printf("\n");
    printf("  ── %s, %s %d %s %d ──\n", label, dow_short(dow), d, mon, y);
    printf("\n");
    {
        char c1[32], c2[32], c3[32], c4[32], c5[32], c6[96];
        pad_left(c1, sizeof(c1), "Hora", 6);
        pad_left(c2, sizeof(c2), "Temp", 7);
        pad_left(c3, sizeof(c3), "Presión", 8);
        pad_left(c4, sizeof(c4), "Humedad", 7);
        pad_left(c5, sizeof(c5), "Lluvia", 7);
        pad_left(c6, sizeof(c6), "Estado", 25);
        printf("  %s  %s  %s  %s  %s  %s\n", c1, c2, c3, c4, c5, c6);
        pad_left(c1, sizeof(c1), "──────", 6);
        pad_left(c2, sizeof(c2), "───────", 7);
        pad_left(c3, sizeof(c3), "────────", 8);
        pad_left(c4, sizeof(c4), "───────", 7);
        pad_left(c5, sizeof(c5), "───────", 7);
        pad_left(c6, sizeof(c6), "─────────────────────────", 25);
        printf("  %s  %s  %s  %s  %s  %s\n", c1, c2, c3, c4, c5, c6);
    }

    for (i = 0; i < g_n_hourly; i++) {
        const Hourly *h = &g_hourly[i];
        char pres[16], prec[16];
        if (strcmp(h->date, today) != 0) continue;
        if (h->pressure != 0) snprintf(pres, sizeof(pres), "%.0f hPa", h->pressure);
        else strcpy(pres, "N/A");
        if (h->precip > 0) snprintf(prec, sizeof(prec), "%.1fmm", h->precip);
        else strcpy(prec, "0.0mm");
        printf("  %02d:00  %5.1f°C  %8s  %3.0f%%  %7s  %s %s\n",
               h->hour, h->temp, pres, h->humid, prec,
               condition_icon(h->condition), h->condition);
    }

    printf("\n");
    printf("  Resumen del día:\n");
    printf("  ─────────────────────────────────────────────────\n");
    printf("  Temperatura\n");
    printf("    Actual:  %.1f°C (%02d:00)\n", t_now, now_h);
    printf("    Máxima:  %.1f°C (%02d:00)\n", t_max, t_max_h);
    printf("    Mínima:  %.1f°C (%02d:00)\n", t_min, t_min_h);
    printf("    Media:   %.1f°C\n", t_mean);
    printf("  Humedad\n");
    printf("    Actual:  %.0f%% (%02d:00)\n", h_now, now_h);
    printf("    Máxima:  %.0f%% (%02d:00)\n", h_max, h_max_h);
    printf("    Mínima:  %.0f%% (%02d:00)\n", h_min, h_min_h);
    printf("    Media:   %.0f%%\n", h_mean);
    if (n_p > 0) {
        printf("  Presión\n");
        printf("    Actual:  %.0f hPa (%02d:00)\n", p_now, now_h);
        printf("    Máxima:  %.0f hPa (%02d:00)\n", p_max, p_max_h);
        printf("    Mínima:  %.0f hPa (%02d:00)\n", p_min, p_min_h);
        printf("    Media:   %.0f hPa\n", p_mean);
    }
    printf("  Estado dom.:  %s %s\n", condition_icon(dominant), dominant);
    printf("  Tendencia:    %s (%+.2f°C/h)\n", temp_trend, trend_slope);
    printf("  Normal julio: %.1f°C | Anomalía: %+.1f°C\n", c->t_mean, t_mean - c->t_mean);

    if (hist.heatwave) {
        if (any_rain) printf("  ⚠ OLA DE CALOR INTERRUMPIDA — lluvias en el día de hoy\n");
        else          printf("  ⚠ OLA DE CALOR: %d días secos consecutivos\n", hist.dry_streak);
    }
    if (night_storm)      printf("  ⛈ Tormentas nocturnas (alta incertidumbre, verificar radar)\n");
    else if (day_storm)   printf("  ⛈ Posibles tormentas durante el día\n");
    if (h_mean >= 70)     printf("  💧 Alta humedad: sensación de bochorno\n");
    if (t_max >= 37)      printf("  🔥 Calor extremo: evitar sol directo 11-17h\n");
    if (any_rain)         printf("  ☂ Lluvias previstas: llevar paraguas\n");

    {
        time_t tt = time(NULL);
        struct tm *tm = localtime(&tt);
        printf("\n  Fuente: Open-Meteo | Actualizado: %02d:%02d CEST\n",
               now_h, tm->tm_min);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/* Reporte principal                                                   */
/* ------------------------------------------------------------------ */
static void run(const Ciudad *c, int days, int show_detail, int resumen) {
    cJSON *raw;
    HistResult hist;
    int i, d;
    char label[128], today[16];

    today_iso(today, sizeof(today));
    ciudad_label(c, label, sizeof(label));

    printf("\n");
    {
        int w;
        for (w = 0; w < 78; w++) printf("=");
        printf("\n  PRONÓSTICO METEOROLÓGICO - %s\n", label);
        for (w = 0; w < 78; w++) printf("=");
        printf("\n");
    }

    {
        time_t tt = time(NULL);
        struct tm *tm = localtime(&tt);
        printf("  Generado: %04d-%02d-%02d %02d:%02d CEST\n",
               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min);
    }
    printf("  Período: %d día(s) | Altitud: %dm | Normal julio: %.1f°C / %dmm\n",
           days, c->alt, c->t_mean, c->precip);
    printf("\n");

    raw = fetch_openmeteo(c, days, HIST_DAYS);
    if (!raw) {
        fprintf(stderr, "\n  ERROR: No se pudieron obtener datos. Verifique conexión.\n");
        return;
    }
    hist = analyze_historical(raw, c);
    build_forecast(raw);
    cJSON_Delete(raw);

    /* ============ SECCIÓN 1: datos observados ============ */
    if (!resumen) {
        print_header("1. DATOS OBSERVADOS");

        if (hist.n_days > 0) {
            printf("\n  Días con datos: %d\n", hist.n_days);
            printf("  Temperatura media observada: %.1f°C\n", hist.mean_obs);
            printf("  Temperatura normal climática: %.1f°C\n", hist.mean_normal);
            printf("  Anomalía térmica: %+.1f°C\n", hist.mean_anomaly);
            printf("  Precipitación total acumulada: %.1f mm\n", hist.total_precip);
            printf("\n");
            printf("  Tendencia reciente: %s\n", hist.trend);
            printf("  Rachas secas consecutivas: %d día(s)\n", hist.dry_streak);
            if (hist.heatwave)
                printf("  ⚠ OLA DE CALOR ACTIVA: %d días secos + anomalía >+3°C\n", hist.dry_streak);

            printf("\n");
    {
        char h1[64], h2[32], h3[32], h4[32], h5[32], h6[32];
        pad_left(h1, sizeof(h1), "Fecha", 14);
                pad_left(h2, sizeof(h2), "T.Min", 7);
                pad_left(h3, sizeof(h3), "T.Max", 7);
                pad_left(h4, sizeof(h4), "T.Media", 8);
                pad_left(h5, sizeof(h5), "Precip", 7);
                pad_left(h6, sizeof(h6), "Desvío", 7);
                printf("  %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6);
                pad_left(h1, sizeof(h1), "──────────────", 14);
                pad_left(h2, sizeof(h2), "───────", 7);
                pad_left(h3, sizeof(h3), "───────", 7);
                pad_left(h4, sizeof(h4), "────────", 8);
                pad_left(h5, sizeof(h5), "───────", 7);
                pad_left(h6, sizeof(h6), "───────", 7);
                printf("  %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6);
            }

            for (i = 0; i < hist.n_days; i++) {
                double tmean = (g_hist_min[i] + g_hist_max[i]) / 2.0;
                double anomaly = tmean - c->t_mean;
                printf("  %-14s %7.1f %7.1f %8.1f°C  %6.1fmm %+6.1f°C\n",
                       g_hist_dates[i], g_hist_min[i], g_hist_max[i],
                       tmean, g_hist_precip[i], anomaly);
            }
        }
    }

    /* ============ SECCIÓN 2: tabla resumen ============ */
    print_header(resumen ? "1. TABLA RESUMEN - PRÓXIMOS DÍAS"
                         : "2. TABLA RESUMEN - PRÓXIMOS DÍAS");

    {
        char h1[64], h2[64], h3[64], h4[64], h5[64], h6[32], h7[96];
        pad_left(h1, sizeof(h1), "Día", 14);
        pad_left(h2, sizeof(h2), "T.Min (h)", 14);
        pad_left(h3, sizeof(h3), "T.Max (h)", 14);
        pad_left(h4, sizeof(h4), "H.min (h)", 14);
        pad_left(h5, sizeof(h5), "H.max (h)", 14);
        pad_left(h6, sizeof(h6), "Lluvia", 8);
        pad_left(h7, sizeof(h7), "Condición", 30);
        printf("  %s %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6, h7);
        pad_left(h1, sizeof(h1), "──────────────", 14);
        pad_left(h2, sizeof(h2), "──────────────", 14);
        pad_left(h3, sizeof(h3), "──────────────", 14);
        pad_left(h4, sizeof(h4), "──────────────", 14);
        pad_left(h5, sizeof(h5), "──────────────", 14);
        pad_left(h6, sizeof(h6), "────────", 8);
        pad_left(h7, sizeof(h7), "──────────────────────────────", 30);
        printf("  %s %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6, h7);
    }

    g_n_stats = 0;
    for (i = 0; i < g_n_fdays; i++) {
        DailyStats *ds = &g_dstats[g_n_stats];
        if (compute_daily_stats(g_fdays[i].date, ds)) {
            const ForecastDay *fd = &g_fdays[i];
            int dy, mo, dd;
            char month[8], wk[8], daylbl[24];
            const char *flag;
            sscanf(fd->date, "%d-%d-%d", &dy, &mo, &dd);
            month_short(fd->date, month, sizeof(month));
            strcpy(wk, dow_short(fd->dow));
            snprintf(daylbl, sizeof(daylbl), "%s %d %s", wk, dd, month);
            pad_left(daylbl, sizeof(daylbl), daylbl, 14);
            flag = ds->t_max >= 38 ? " 🔥" : "";
            printf("  %s %5.1f°C (%02d:00)  %5.1f°C (%02d:00)  "
                   "%3.0f%% (%02d:00)  %3.0f%% (%02d:00)  "
                   "%6.2fmm  %s %s%s\n",
                   daylbl, ds->t_min, ds->t_min_hour, ds->t_max, ds->t_max_hour,
                   ds->h_min, ds->h_min_hour, ds->h_max, ds->h_max_hour,
                   ds->precip, condition_icon(ds->condition), ds->condition, flag);
            g_n_stats++;
        }
    }

    printf("\n  🔥 = T ≥ 38°C\n");

    /* ============ SECCIÓN 3: anomalías ============ */
    if (!resumen) {
        print_header("3. ANÁLISIS DE ANOMALÍAS Y TENDENCIA");

        if (hist.n_days > 0) {
            printf("\n  Temperatura media observada (histórico): %.1f°C\n", hist.mean_obs);
            printf("  Temperatura normal climática (julio):   %.1f°C\n", hist.mean_normal);
            printf("  Anomalía:                              %+.1f°C\n", hist.mean_anomaly);

            if (hist.heatwave) {
                printf("\n  ⚠ OLA DE CALOR ACTIVA: %d días sin precipitación\n", hist.dry_streak);
                printf("     con anomalía sostenida >+3°C sobre la normal histórica\n");
            } else {
                printf("\n  Sin condiciones de ola de calor detectadas.\n");
            }

            if (!strcmp(hist.trend, "rapid_warming"))      printf("  Tendencia: CALENTAMIENTO RÁPIDO (>1.5°C/3días)\n");
            else if (!strcmp(hist.trend, "warming"))       printf("  Tendencia: CALENTAMIENTO MODERADO\n");
            else if (!strcmp(hist.trend, "cooling"))       printf("  Tendencia: ENFRIAMIENTO\n");
            else if (!strcmp(hist.trend, "rapid_cooling")) printf("  Tendencia: ENFRIAMIENTO RÁPIDO\n");
            else                                           printf("  Tendencia: ESTABLE\n");
        }

        printf("\n  Anomalías proyectadas por día (normal julio: %.1f°C):\n", c->t_mean);
        {
            char h1[64], h2[32], h3[32], h4[32], h5[32];
            pad_left(h1, sizeof(h1), "Día", 14);
            pad_left(h2, sizeof(h2), "T.Media", 9);
            pad_left(h3, sizeof(h3), "Normal", 8);
            pad_left(h4, sizeof(h4), "Anomalía", 9);
            pad_left(h5, sizeof(h5), "Lluvia", 8);
            printf("  %s %s %s %s %s\n", h1, h2, h3, h4, h5);
            pad_left(h1, sizeof(h1), "──────────────", 14);
            pad_left(h2, sizeof(h2), "─────────", 9);
            pad_left(h3, sizeof(h3), "────────", 8);
            pad_left(h4, sizeof(h4), "─────────", 9);
            pad_left(h5, sizeof(h5), "────────", 8);
            printf("  %s %s %s %s %s\n", h1, h2, h3, h4, h5);
        }
        for (d = 0; d < g_n_stats; d++) {
            const ForecastDay *fd = &g_fdays[d];
            const DailyStats *ds = &g_dstats[d];
            int dy, mo, dd;
            char wk[8], daylbl[24];
            double anomaly;
            sscanf(fd->date, "%d-%d-%d", &dy, &mo, &dd);
            strcpy(wk, dow_short(fd->dow));
            snprintf(daylbl, sizeof(daylbl), "%s %d", wk, dd);
            pad_left(daylbl, sizeof(daylbl), daylbl, 14);
            anomaly = ds->t_mean - c->t_mean;
            printf("  %s %7.1f°C  %6.1f°C  %+7.1f°C  %6.1fmm\n",
                   daylbl, ds->t_mean, c->t_mean, anomaly, fd->precip_sum);
        }
    }

    /* ============ SECCIÓN 4: zancudos ============ */
    {
        MosquitoRisk mr = compute_mosquito_risk(&hist);

        if (!resumen) {
            const char *lvl = mosquito_level(mr.avg_risk);
            const char *level_icon = !strcmp(lvl, "ALTO") ? "🔴 ALTO" :
                                     !strcmp(lvl, "MEDIO") ? "🟡 MEDIO" :
                                     !strcmp(lvl, "BAJO")  ? "🟢 BAJO" : "⚪ MUY BAJO";
            print_header("4. ÍNDICE DE RIESGO DE ZANCUDOS");
            printf("\n  Riesgo promedio período: %.1f/10  (%s)\n", mr.avg_risk, level_icon);
            printf("  Tendencia: %s\n", mr.trend);
            printf("  Pico de riesgo: día %d (%.1f/10)\n", mr.peak_day + 1, mr.peak_risk);

            {
                char h1[64], h2[32], h3[64], h4[32], h5[32], h6[32];
                pad_left(h1, sizeof(h1), "Día", 14);
                pad_left(h2, sizeof(h2), "Riesgo", 8);
                pad_left(h3, sizeof(h3), "Nivel", 14);
                pad_left(h4, sizeof(h4), "T.Media", 8);
                pad_left(h5, sizeof(h5), "H.Media", 8);
                pad_left(h6, sizeof(h6), "Lluvia", 8);
                printf("  %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6);
                pad_left(h1, sizeof(h1), "──────────────", 14);
                pad_left(h2, sizeof(h2), "────────", 8);
                pad_left(h3, sizeof(h3), "──────────────", 14);
                pad_left(h4, sizeof(h4), "────────", 8);
                pad_left(h5, sizeof(h5), "────────", 8);
                pad_left(h6, sizeof(h6), "────────", 8);
                printf("  %s %s %s %s %s %s\n", h1, h2, h3, h4, h5, h6);
            }
            for (d = 0; d < g_n_stats; d++) {
                const ForecastDay *fd = &g_fdays[d];
                const DailyStats *ds = &g_dstats[d];
                int dy, mo, dd;
                char wk[8], daylbl[24];
                double risk = mr.daily_risks[d];
                double h_mean = (ds->h_min + ds->h_max) / 2.0;
                const char *rl = mosquito_level(risk);
                const char *ric = !strcmp(rl, "ALTO") ? "🔴 ALTO" :
                                  !strcmp(rl, "MEDIO") ? "🟡 MEDIO" :
                                  !strcmp(rl, "BAJO")  ? "🟢 BAJO" : "⚪ MUY BAJO";
                sscanf(fd->date, "%d-%d-%d", &dy, &mo, &dd);
                strcpy(wk, dow_short(fd->dow));
                snprintf(daylbl, sizeof(daylbl), "%s %d", wk, dd);
                pad_left(daylbl, sizeof(daylbl), daylbl, 14);
                printf("  %s %5.1f/10  %-14s %6.1f°C  %5.0f%%  %5.1fmm\n",
                       daylbl, risk, ric, ds->t_mean, h_mean, fd->precip_sum);
            }

            printf("\n  %s Tendencia: %s\n",
                   !strcmp(mr.trend, "aumentando") ? "⬆" :
                   !strcmp(mr.trend, "disminuyendo") ? "⬇" : "➡",
                   mr.trend);
            printf("  🦟 Riesgo basado en: temperatura (45%%), humedad (35%%), precipitación (20%%)\n");
            printf("  ℹ Ciclo óptimo: T 25-30°C, humedad >70%%, agua estancada post-lluvia\n");
        }

        /* ============ SECCIÓN 5: tormentas ============ */
        {
            StormResult sr = analyze_convective_risk();

            if (sr.has_any_storm && !resumen) {
                print_header("5. ANÁLISIS DE TORMENTAS CONVECTIVAS");
                for (d = 0; d < g_n_stats; d++) {
                    const ForecastDay *fd = &g_fdays[d];
                    StormDay *sd = &sr.daily[d];
                    char hours_str[128];
                    int hh;
                    if (!sd->has_storm) continue;
                    hours_str[0] = '\0';
                    for (hh = 0; hh < sd->n_hours; hh++) {
                        char tmp[8];
                        snprintf(tmp, sizeof(tmp), "%02d:00", sd->hours[hh]);
                        if (hh > 0) strcat(hours_str, ", ");
                        strcat(hours_str, tmp);
                    }
                    printf("  %s %s: Tormenta %s (%.1fmm) a las %s%s\n",
                           condition_icon("Tormenta"), fd->date, sd->severity,
                           sd->conv_precip, hours_str,
                           sd->night_storm ? " 🌙" : "");
                }
                if (sr.night_storm_days > 0)
                    printf("\n  🌙 Tormentas nocturnas (%d día(s)): difíciles de predecir con >12h de antelación\n",
                           sr.night_storm_days);
                if (sr.severe_days > 0)
                    printf("  ⚠ Tormentas fuertes/severas: posible granizo, ráfagas y actividad eléctrica intensa\n");

                printf("\n  ℹ Las tormentas convectivas son eventos localizados y de alta incertidumbre.\n");
                printf("     El modelo puede subestimar su intensidad si faltan datos de CAPE/CIN.\n");
                printf("     Se recomienda verificar fuentes locales (protección civil, radar meteorológico).\n");
            }

            /* ============ SECCIÓN 6: resumen ejecutivo ============ */
            print_header(resumen ? "2. RESUMEN EJECUTIVO" : "6. RESUMEN EJECUTIVO");

            {
                double total_precip_proj = 0, mean_forecast_temp = 0;
                double overall_max = -1e9, overall_min = 1e9;
                const char *trend_icon;
                char mosq_label[64];
                int any_extreme_heat = 0, any_hot = 0, any_high_hum = 0;

                for (d = 0; d < g_n_fdays; d++) total_precip_proj += g_fdays[d].precip_sum;
                for (d = 0; d < g_n_stats; d++) {
                    mean_forecast_temp += g_dstats[d].t_mean;
                    if (g_dstats[d].t_max > overall_max) overall_max = g_dstats[d].t_max;
                    if (g_dstats[d].t_min < overall_min) overall_min = g_dstats[d].t_min;
                    if (g_dstats[d].t_max >= 37) any_extreme_heat = 1;
                    if (g_dstats[d].t_max >= 35) any_hot = 1;
                    if (g_dstats[d].h_max >= 80) any_high_hum = 1;
                }
                if (g_n_stats > 0) mean_forecast_temp /= g_n_stats;

                printf("\n  Ciudad: %s | Altitud: %dm snm\n",
                       label, c->alt);
                printf("  Período: %d día(s) | Generado: ", days);
                {
                    time_t tt = time(NULL);
                    struct tm *tm = localtime(&tt);
                    printf("%04d-%02d-%02d %02d:%02d CEST\n",
                           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                           tm->tm_hour, tm->tm_min);
                }
                printf("  Fuentes: Open-Meteo (DWD/MeteoFrance/MetOffice), datos observados, normales climáticas\n");
                printf("  Método: Integración multi-modelo + análisis estadístico de anomalías\n");

                printf("\n  Temperatura promedio período:  %5.1f°C\n", mean_forecast_temp);
                printf("  Temperatura máxima absoluta:   %5.1f°C\n", overall_max);
                printf("  Temperatura mínima absoluta:   %5.1f°C\n", overall_min);
                printf("  Precipitación total estimada:  %5.1f mm\n", total_precip_proj);

                if (total_precip_proj == 0)      printf("  🏜 Sin precipitación prevista\n");
                else if (total_precip_proj < 2)  printf("  🌦 Lluvias ligeras aisladas\n");
                else if (total_precip_proj < 10) printf("  🌧 Lluvias moderadas\n");
                else                             printf("  ⛈ Precipitación significativa\n");

                if (hist.heatwave && total_precip_proj > 0)
                    printf("  ⚠ OLA DE CALOR INTERRUMPIDA (%d días secos hasta ayer — lluvias previstas en el período)\n",
                           hist.dry_streak);
                else if (hist.heatwave)
                    printf("  ⚠ OLA DE CALOR activa (%d días secos)\n", hist.dry_streak);

                trend_icon = !strcmp(mr.trend, "aumentando") ? "⬆" :
                             !strcmp(mr.trend, "disminuyendo") ? "⬇" : "➡";
                snprintf(mosq_label, sizeof(mosq_label), "%s %s", mosquito_level(mr.avg_risk), trend_icon);
                printf("  🦟 Riesgo zancudos: %.1f/10 (%s)\n", mr.avg_risk, mosq_label);
                if (sr.has_any_storm) {
                    printf("  ⛈ Tormentas previstas (%d fuerte(s), %d nocturna(s))%s%s\n",
                           sr.severe_days, sr.night_storm_days,
                           sr.severe_days > 0 ? " 🌩" : "",
                           sr.night_storm_days > 0 ? " 🌙" : "");
                }

                printf("\n  Recomendaciones:\n");
                if (sr.has_any_storm) {
                    printf("    ⛈ Tormentas: evitar exteriores durante actividad eléctrica, buscar refugio\n");
                    if (sr.night_storm_days > 0)
                        printf("    🌙 Tormentas nocturnas: asegurar objetos sueltos en balcones/terrazas\n");
                }
                if (any_extreme_heat)
                    printf("    🔥 Días de calor extremo: mantenerse hidratado, evitar sol directo 11-17h\n");
                if (total_precip_proj > 0)
                    printf("    ☂  Llevar paraguas día(s) con precipitación prevista\n");
                if (any_hot)
                    printf("    🧴 Protección solar SPF 50+ obligatoria, sombrero, gafas de sol\n");
                if (any_high_hum)
                    printf("    💧 Alta humedad: posible sensación de bochorno\n");
                if (mr.avg_risk >= 4)
                    printf("    🦟 Riesgo zancudos MEDIO+ : usar repelente, evitar agua estancada\n");
                printf("    🌬 Ventilar durante la noche/madrugada para refrescar viviendas\n");

                {
                    const DailyStats *hottest = NULL;
                    const ForecastDay *hfd = NULL;
                    for (d = 0; d < g_n_stats; d++) {
                        if (!hottest || g_dstats[d].t_max > hottest->t_max) {
                            hottest = &g_dstats[d];
                            hfd = &g_fdays[d];
                        }
                    }
                    if (hottest)
                        printf("\n  ⚠ Pico de calor: %.0f°C el %s a las %02d:00\n",
                               hottest->t_max, hfd->date, hottest->t_max_hour);
                }

                printf("\n  %s\n", "──────────────────────────────────────────────────────────────────────────────");
                printf("  Datos: Open-Meteo API (libre, sin API key) | Modelos: ECMWF/GFS/ICON\n");
                printf("  Próxima actualización recomendada: en +6h para mejor precisión\n");
            }
        }
    }

    /* ============ SECCIÓN 7: detalle horario ============ */
    if (show_detail) {
        print_header("7. PREDICCIÓN HORARIA DETALLADA (--extendido / -e)");
        {
            char current_date[16] = "";
            int row_count = 0;
            for (i = 0; i < g_n_hourly; i++) {
                const Hourly *h = &g_hourly[i];
                char precip_str[16], week[16], mons[8];
                int dy, mo, dd;
                if (strcmp(h->date, today) < 0) continue;
                if (row_count >= days * 24) break;

                if (strcmp(h->date, current_date) != 0) {
                    strcpy(current_date, h->date);
                    sscanf(h->date, "%d-%d-%d", &dy, &mo, &dd);
                    dow_name(h->date, week, sizeof(week));
                    month_short(h->date, mons, sizeof(mons));
                    printf("\n  ── %s %d %s %d ──\n",
                           dow_short_upper(week), dd, mons, dy);
                }

                if (h->precip >= 0.1)
                    snprintf(precip_str, sizeof(precip_str), "%.1f", h->precip);
                else if (h->precip_prob >= 50)
                    snprintf(precip_str, sizeof(precip_str), "~%.1f", h->precip);
                else
                    strcpy(precip_str, "0.0");

                printf("  %s  %5.1f°C  %3.0f%%  %5smm  %s %s\n",
                       h->iso, h->temp, h->humid, precip_str,
                       condition_icon(h->condition), h->condition);
                row_count++;
            }
            printf("\n  Total: %d registros horarios\n", row_count);
        }
    }

    printf("\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
static void usage(const char *prog) {
    printf("Pronóstico meteorológico para ciudades de Italia v%s (C)\n", VERSIONA);
    printf("Uso: %s [--ciudad CIUDAD] [--dias N] [--extendido] [--today] [--resumen] [--list] [--version]\n", prog);
    printf("\nEjemplos:\n");
    printf("  %s                                  # %s 5 días\n", prog, g_hay_config ? "ciudad por defecto" : "Torino 5 días");
    printf("  %s -v / --version                   # mostrar versión\n", prog);
    printf("  %s -c ROMA                         # Roma 5 días\n", prog);
    printf("  %s --today                          # hoy (hora por hora)\n", prog);
    printf("  %s -t -c NAPOLI                    # hoy Napoli\n", prog);
    printf("  %s -c MILANO -d 10                 # Milano 10 días\n", prog);
    printf("  %s -c NAPOLI -e                    # Napoli + detalle horario\n", prog);
    printf("  %s -l                               # listar ciudades (de %s)\n", prog, CONFIG_FILE);
    printf("  %s -r                               # solo resumen\n", prog);
    printf("  %s -r -c MILANO -d 10              # resumen Milano 10 días\n", prog);
    printf("\nParámetros:\n");
    printf("  -c, --ciudad CIUDAD    Ciudad a consultar (por defecto la de %s)\n", CONFIG_FILE);
    printf("  -d, --dias DIAS         días de pronóstico (1-16, por defecto 5)\n");
    printf("  -e, --extendido        muestra el detalle horario completo\n");
    printf("  -t, --today            pronóstico de hoy hora por hora\n");
    printf("  -r, --resumen          solo tabla resumen y análisis\n");
    printf("  -l, --list              lista las ciudades definidas en %s\n", CONFIG_FILE);
    printf("  -v, --version          muestra la versión del programa\n");
    printf("  -h, --help             esta ayuda\n");

    printf("\n══ Configuración: %s ══\n", CONFIG_FILE);
    printf("  Las ciudades, sus coordenadas y su región se definen en el\n");
    printf("  archivo %s (una por línea, formato\n", CONFIG_FILE);
    printf("  NOMBRE;lat,lon;REGION). Debe estar al lado de este programa\n");
    printf("  ─o en el directorio actual─ para ser leído.\n");

    printf("\n  AÑADIR una ciudad nueva:\n");
    printf("   1) Obten sus coordenadas: en Google Maps haz clic derecho sobre\n");
    printf("      el sitio y copia el par \"lat,lon\" (p. ej. 46.49,11.36).\n");
    printf("   2) Añade una línea al final de %s con el formato\n", CONFIG_FILE);
    printf("        NOMBRE;lat,lon;REGION\n");
    printf("      p. ej.   BOLZANO;46.49,11.36;Trentino-Alto Adige\n");
    printf("      usa '.' como separador decimal (no coma): 46.49,11.36\n");
    printf("      La REGION es opcional: si se omite, el programa mostrará\n");
    printf("      solo la ciudad y las coordenadas.\n");

    printf("\n  QUITAR una ciudad:\n");
    printf("   borra su línea en %s para eliminarla por completo.\n", CONFIG_FILE);
    printf("   Si solo la precedes con '#' deja de ser por defecto, pero\n");
    printf("   seguirá disponible y visible en '%s -l'.\n\n", prog);

    printf("\n  CAMBIAR la ciudad por defecto:\n");
    printf("   la ciudad por defecto es la ÚNICA línea SIN '#'. Deja '#' a\n");
    printf("   las demás y solo una sin él:\n");
    printf("        #TORINO;45.07,7.67;Piemonte\n");
    printf("        MILANO;45.46,9.19;Lombardia        <- por defecto\n");
    printf("   Nota: la línea por defecto es la que se usa al ejecutar sin\n");
    printf("   '-c'. Ver también '%s -l' para ver lo cargado.\n\n", prog);
}

/* ------------------------------------------------------------------ */
/* Tipografía de ancho fijo (solo aplicable en la consola de Windows)  */
/* ------------------------------------------------------------------ */
/* En Linux/macOS la fuente la elige el emulador de terminal (que por  */
/* defecto es monoespaciada: Menlo en macOS, DejaVu Sans Mono etc.).   */
/* En Windows la consola puede abrir con fuente variable: aquí se      */
/* fuerza una fuente monoespaciada del sistema (Cascadia/Consolas).    */
#ifdef _WIN32
static void set_monospace_font(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX cf;
    static const wchar_t *mono[] = {
        L"Cascadia Mono", L"Consolas", L"Courier New"
    };
    DWORD n, i;
    if (hOut == INVALID_HANDLE_VALUE) return;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    if (!GetCurrentConsoleFontEx(hOut, FALSE, &cf)) return;
    cf.dwFontSize.Y = 0;               /* mantener tamaño por defecto   */
    cf.FontFamily = FF_MODERN;         /* familia de ancho fijo         */
    cf.FontWeight = FW_NORMAL;
    for (n = 0, i = 0; i < sizeof(mono) / sizeof(mono[0]); i++) {
        wcscpy(cf.FaceName, mono[i]);
        if (SetCurrentConsoleFontEx(hOut, FALSE, &cf)) { n = 1; break; }
    }
    (void)n;
}
#endif

int main(int argc, char **argv) {
    const char *ciudad_default;
    const char *ciudad;
    int days = 5;
    int show_detail = 0;
    int do_list = 0;
    int do_today = 0;
    int resumen = 0;
    const Ciudad *c;
    int i;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    set_monospace_font();
#endif

    /* Cargar ciudades y coord de clima.conf (o las 20 capitales si no hay) */
    ciudad_default = cargar_config();     /* clave por defecto o NULL       */
    if (!ciudad_default) ciudad_default = cargar_fallback();
    ciudad = ciudad_default ? ciudad_default : "TORINO";

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--ciudad") || !strcmp(a, "-c")) {
            if (i + 1 < argc) ciudad = argv[++i];
        } else if (!strcmp(a, "--dias") || !strcmp(a, "-d")) {
            if (i + 1 < argc) days = atoi(argv[++i]);
        } else if (!strcmp(a, "--extendido") || !strcmp(a, "-e")) {
            show_detail = 1;
        } else if (!strcmp(a, "--list") || !strcmp(a, "-l")) {
            do_list = 1;
        } else if (!strcmp(a, "--today") || !strcmp(a, "-t")) {
            do_today = 1;
        } else if (!strcmp(a, "--resumen") || !strcmp(a, "-r")) {
            resumen = 1;
        } else if (!strcmp(a, "--version") || !strcmp(a, "-v")) {
            printf("Analisis-de-Clima %s (C)\n", VERSIONA);
            return 0;
        } else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Parámetro desconocido: %s\n", a);
            usage(argv[0]);
            return 1;
        }
    }

    if (do_list) {
        list_ciudades();
        return 0;
    }

    c = find_ciudad(ciudad);
    if (!c) {
        fprintf(stderr, "ERROR: ciudad no válida: %s\n", ciudad);
        list_ciudades();
        return 1;
    }

    if (do_today) {
        today_report(c);
        return 0;
    }

    if (days < 1 || days > 16) {
        fprintf(stderr, "ERROR: dias debe estar entre 1 y 16 (límite Open-Meteo gratuito)\n");
        return 1;
    }

    run(c, days, show_detail, resumen);
    return 0;
}
