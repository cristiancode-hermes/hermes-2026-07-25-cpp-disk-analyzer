# diskwatch — Linux Disk I/O Monitor

Monitor de disco Linux en C++23 que lee `/proc/diskstats` y presenta estadísticas de I/O.

## Características

- **Lectura en tiempo real** de estadísticas de disco desde `/proc/diskstats`
- **Información de dispositivo** desde `/sys/block/<dev>/` (tamaño, modelo, SSD/HDD)
- **Salida formateada** en tabla legible, JSON, o resumen
- **Modo watch** con delta IOPS y throughput en tiempo real
- **Exclusión automática** de dispositivos virtuales (ram, loop, dm-)

## Build

```bash
cmake --preset debug
cmake --build --preset debug
```

## Uso

```bash
# Tabla de todos los discos
./build/debug/diskwatch

# Salida JSON para procesamiento
./build/debug/diskwatch --json

# Modo watch cada 2 segundos, 5 iteraciones
./build/debug/diskwatch --watch 2000 --count 5

# Resumen agregado
./build/debug/diskwatch --summary

# Incluir dispositivos virtuales
./build/debug/diskwatch --all

# Valores en crudo (sin formato legible)
./build/debug/diskwatch --raw
```

## Tests

```bash
cd build/debug && ctest --output-on-failure
```

## Tecnologías

- C++23
- CLI11 (argument parsing, FetchContent)
- nlohmann/json (salida JSON)
- spdlog (logging)
- Catch2 (tests, v3)
