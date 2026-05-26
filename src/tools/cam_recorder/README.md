# Grabador de Cámara CLI (cam_recorder)

Este es un grabador de cámara web simple y potente desarrollado en C++17 utilizando OpenCV para el procesamiento de video, `argparse` para la interfaz de línea de comandos, e `indicators` para una barra de progreso premium en la terminal.

## Características

- Grabación de cámara web con duración configurable.
- Barra de progreso premium de color azul con visualización de porcentaje, tiempo transcurrido, tiempo restante y duración total.
- Configuración opcional de FPS y resolución personalizada.
- Opción para listar todas las cámaras detectadas en el sistema.
- Diseño modular que sigue los principios SOLID (Patrón Command).

---

## Requisitos / Requirements

- **C++17 Compiler** (GCC 7+, MSYS2/MinGW en Windows).
- **OpenCV 4.x**
- **Make**

---

## Compilación / Build Instructions

### En Windows (MSYS2 UCRT64 / MinGW):
Asegúrate de estar en el directorio `src/tools/cam_recorder` de tu consola MSYS2 y ejecuta:
```bash
make
```

### En Linux:
Instala las dependencias y ejecuta:
```bash
make
```

---

## Uso / Usage

Una vez compilado, puedes ejecutar la aplicación directamente desde la línea de comandos.

### 1. Ayuda general / General Help
```bash
./cam_recorder --help
```

### 2. Listar cámaras web detectadas / List available cameras
```bash
./cam_recorder --list
# o con la opción corta
./cam_recorder -l
```

### 3. Grabar video predeterminado / Record with default settings
Graba por defecto de la cámara `0` durante `10` segundos con salida a `out.mp4`:
```bash
./cam_recorder
```

### 4. Grabación personalizada / Custom recording
Graba de la cámara ID `1` durante `15` segundos, guardando en `mi_video.mp4` a 30 FPS con resolución 1280x720:
```bash
./cam_recorder --device 1 --duration 15 --output mi_video.mp4 --fps 30 --width 1280 --height 720
# o usando las opciones cortas
./cam_recorder -d 1 -t 15 -o mi_video.mp4 -f 30 -w 1280 -h 720
```

---

## Estructura del Proyecto / Project Structure

- `cam_recorder.cpp`: Código fuente principal de la aplicación.
- `Makefile`: Archivo de compilación portable.
- `requirements.txt`: Requisitos de la aplicación.
- `../../common/inc/`: Directorio de dependencias comunes (`argparse.hpp`, `indicators.hpp`).
