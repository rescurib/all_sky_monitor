# Nombre del proyecto
PROJECT_NAME = cam_calibrator

# Archivos fuente
SRC = $(PROJECT_NAME).cpp

# Ejecutable de salida
TARGET = $(PROJECT_NAME)

# Detectar OpenCV usando pkg-config
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
