# Makefile para ExtreamFS Backend

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0 -fPIC
INCLUDE_DIR = include
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib

# Archivos fuente
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))

# Nombres de salida
TARGET = $(BIN_DIR)/extremsfs
LIBRARY = $(LIB_DIR)/libextremsfs.a

# Targets
.PHONY: all clean help run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	@echo "Enlazando $@..."
	@$(CXX) $(CXXFLAGS) -o $@ $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS)) $(OBJ_DIR)/main.o -lstdc++fs

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(OBJ_DIR)
	@echo "Compilando $<..."
	@$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	@echo "Limpiando..."
	@rm -rf $(BUILD_DIR)
	@echo "Limpieza completada."

help:
	@echo "Comandos disponibles:"
	@echo "  make          - Compila el proyecto"
	@echo "  make clean    - Elimina archivos compilados"
	@echo "  make run      - Ejecuta la aplicación"
	@echo "  make help     - Muestra esta ayuda"

run: $(TARGET)
	@echo "Ejecutando $(TARGET)..."
	@$(TARGET)

.SILENT: help
