# Nombre del proyecto
PROJECT_NAME = starfield_match

# Archivos fuente
SRC = $(PROJECT_NAME).cpp

# Ejecutable de salida
TARGET = $(PROJECT_NAME)

# Detectar OpenCV usando pkg-config (Linux/Mac) o variables de entorno (Windows)
# Para Windows, se recomienda definir OPENCV_DIR y agregar las rutas de include/lib manualmente si no se usa pkg-config

# Variables para OpenCV (ajustar según tu instalación si es necesario)
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4 2>/dev/null)
OPENCV_LIBS   = $(shell pkg-config --libs opencv4 2>/dev/null)

# Compilador y flags
CXX = g++
CXXFLAGS = -std=c++17 $(OPENCV_CFLAGS) -I../../common/inc
LDFLAGS = $(OPENCV_LIBS)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET).exe
