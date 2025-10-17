# Compilador
CXX = g++



# Nombre del ejecutable
TARGET = proyecto


# Regla por defecto
all: $(TARGET)

$(TARGET) : main.cpp proyecto.o
	$(CXX) main.cpp proyecto.o -o $(TARGET)

proyecto.o : proyecto.h proyecto.cpp
	$(CXX) -c proyecto.cpp 

clean: 
	rm -f $(TARGET) ./*.o
.PHONY: all clean rebuild run