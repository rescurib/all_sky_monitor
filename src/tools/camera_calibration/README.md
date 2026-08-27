# Calibrador de Cámara ChArUco (cam_calibrator)

Esta herramienta de línea de comandos en C++ permite realizar la calibración intrínseca de una cámara utilizando un tablero ChArUco. La aplicación captura de forma automatizada un número específico de fotogramas del patrón ChArUco, calcula los parámetros intrínsecos de la cámara y los coeficientes de distorsión, y finalmente guarda el modelo en formatos JSON y Markdown.

## Características

- Captura de fotogramas automatizada basada en intervalos de tiempo configurables.
- Activación automática: el ciclo de captura comienza inmediatamente tras la primera detección exitosa del patrón ChArUco.
- Retroalimentación en tiempo real: superposiciones en la ventana del flujo de video indicando el progreso de la calibración, el tiempo restante para el siguiente disparo automático, y dibujo de los puntos y bordes detectados.
- Generación de reportes limpios y listos para producción tanto en formato estructurado JSON como en reporte visual Markdown.

## Requisitos / Requirements

- **Compilador C++17** (MSYS2/MinGW en Windows o GCC/Clang en Linux).
- **OpenCV 4.x** (incluyendo el módulo ArUco/ChArUco).
- **Make**

## Compilación / Build Instructions

Asegúrate de estar en el directorio `src/tools/camera_calibration` y ejecuta en tu terminal:

```bash
make
```

Esto generará el ejecutable `cam_calibrator` (o `cam_calibrator.exe` en Windows).

## Uso / Usage

Ejecuta el binario directamente desde la consola con la opción `--help` para conocer todas las opciones:

```bash
./cam_calibrator --help
```

### Argumentos de Línea de Comandos

- `-d`, `--device`: ID del dispositivo de cámara web a utilizar (por defecto: `0`).
- `-n`, `--num-images`: Número de imágenes con tablero detectado requeridas para la calibración (por defecto: `15`).
- `-i`, `--interval`: Intervalo automático de disparo en segundos entre capturas (por defecto: `2.0`).
- `-c`, `--cols`: Número de columnas del tablero ChArUco (número de cuadrados en el eje X, por defecto: `8`).
- `-r`, `--rows`: Número de filas del tablero ChArUco (número de cuadrados en el eje Y, por defecto: `11`).
- `-s`, `--square-size`: Tamaño del lado de un cuadrado en metros (por defecto: `0.030`, es decir, 30mm).
- `-m`, `--marker-size`: Tamaño del lado del marcador ArUco interno en metros (por defecto: `0.015`, es decir, 15mm).
- `-o`, `--output`: Nombre base para los archivos de salida del modelo (por defecto: `camera_model`). Genera `camera_model.json` y `camera_model.md`.
- `-w`, `--width`: Ancho de resolución de la cámara.
- `-h`, `--height`: Alto de resolución de la cámara.

### Ejemplo de Ejecución

Calibrar la cámara `0` capturando `15` imágenes a un intervalo de `2.5` segundos, usando un tablero de `8x11` y guardando el resultado como `modelo_camara.json` y `modelo_camara.md`:

```bash
./cam_calibrator -d 0 -n 15 -i 2.5 -c 8 -r 11 -s 0.030 -m 0.015 -o modelo_camara
```

### Uso con el patrón generado por `generate_pattern.py`

Si generaste el tablero con este comando:

```bash
python generate_pattern.py -o "D:\Proyectos\Sky_Monitor\Calibration Patterns\pattern-tools\charuco_13x18.svg" -u mm -w 130 -h 180 --rows 8 --columns 6 -T charuco_board --square_size 20 -p 14 -f DICT_5X5_100.json.gz
```

entonces la configuración que debe usarse en `cam_calibrator` es:

- `-c 6` y `-r 8` porque el patrón tiene 6 columnas y 8 filas de cuadrados.
- `-s 0.020` porque `20 mm = 0.020 m`.
- `-m 0.014` porque `14 mm = 0.014 m`.
- El diccionario usado por el generador es `DICT_5X5_100`, y este programa usa ese mismo diccionario por defecto.
- Los parámetros `-w` y `-h` del generador corresponden al tamaño del papel/salida SVG, no a la resolución de la cámara; para la cámara usa `-w` y `-h` de la resolución si quieres cambiarla.

Ejemplo de ejecución compatible con ese tablero:

```bash
./cam_calibrator -d 0 -n 15 -i 2.0 -c 6 -r 8 -s 0.020 -m 0.014 -o modelo_charuco_6x8
```

Si tu cámara usa otra resolución, puedes añadir también:

```bash
./cam_calibrator -d 0 -n 15 -i 2.0 -c 6 -r 8 -s 0.020 -m 0.014 -w 1280 -h 720 -o modelo_charuco_6x8
```

Al iniciar, la cámara mostrará el video en vivo. Una vez que sostengas el tablero de manera visible para la cámara, el calibrador comenzará a capturar automáticamente los fotogramas hasta completar el número configurado, aplicando un parpadeo visual en cada foto guardada.
