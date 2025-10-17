#ifndef FILESYSTEMSIMULATOR_H
#define FILESYSTEMSIMULATOR_H

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cmath>
#include <iomanip>

// --- Constantes Globales para la Simulación ---
const int DISK_BLOCKS = 256;         // Número total de bloques en nuestro disco virtual.
const int BLOCK_SIZE = 64;           // Tamaño de cada bloque en bytes (para simular contenido).
const int FAT_EOF = -1;              // Marcador de Fin de Archivo para la tabla FAT.
const int FAT_FREE = 0;              // Marcador de bloque libre para la tabla FAT.
const int NUM_DIRECT_POINTERS = 4;   // Número de punteros directos en un inodo ext2.
const int POINTERS_PER_BLOCK = BLOCK_SIZE / sizeof(int); // Cuántos punteros caben en un bloque.

// --- Estructura para un Bloque del Disco ---
// Representa la unidad mínima de almacenamiento en nuestro disco virtual.
struct Block {
    char data[BLOCK_SIZE]; // Contenido del bloque.
    bool is_free = true;   // Estado del bloque.

    Block() {
        std::fill(data, data + BLOCK_SIZE, 0); // Inicializa el bloque vacío.
    }
};

// --- Clase Disco ---
// Simula el hardware del disco duro, gestionando la colección de bloques.
class Disk {
public:
    // El disco es un vector de bloques.
    std::vector<Block> blocks;

    Disk() : blocks(DISK_BLOCKS) {}

    // Encuentra el primer bloque libre y devuelve su índice.
    int findFreeBlock() {
        for (int i = 0; i < DISK_BLOCKS; ++i) {
            if (blocks[i].is_free) {
                return i;
            }
        }
        return -1; // No hay bloques libres.
    }

    // Lee el contenido de un bloque.
    std::string readBlock(int block_index) {
        return std::string(blocks[block_index].data, BLOCK_SIZE);
    }

    // Escribe datos en un bloque.
    void writeBlock(int block_index, const std::string& data_str) {
        blocks[block_index].is_free = false;
        // Copia los datos, truncando si es necesario.
        std::copy(data_str.begin(), data_str.begin() + std::min((size_t)BLOCK_SIZE, data_str.length()), blocks[block_index].data);
    }

    // Libera un bloque.
    void freeBlock(int block_index) {
        blocks[block_index].is_free = true;
        std::fill(blocks[block_index].data, blocks[block_index].data + BLOCK_SIZE, 0);
    }
};

// --- Estructura para un Inodo (usado por Ext2) ---
// Contiene los metadatos y punteros de un archivo en el sistema ext2.
struct Inode {
    bool is_used = false;
    int size = 0;
    std::vector<int> direct_pointers; // Punteros directos a bloques de datos.
    int single_indirect = -1;       // Puntero a un bloque que contiene más punteros.
    // El doble indirecto se omite por simplicidad en esta simulación.

    Inode() : direct_pointers(NUM_DIRECT_POINTERS, -1) {}
};

// --- Interfaz del Sistema de Archivos (Clase Base Abstracta) ---
// Define las operaciones que CUALQUIER sistema de archivos debe implementar.
class FileSystem {
public:
    // Usamos un puntero al disco para que ambos sistemas puedan operar sobre el mismo "hardware".
    std::shared_ptr<Disk> disk;

    FileSystem(std::shared_ptr<Disk> d) : disk(d) {}
    virtual ~FileSystem() = default;

    // Métodos virtuales puros que deben ser implementados por las clases hijas.
    virtual bool create(const std::string& filename, const std::string& content) = 0;
    virtual std::string read(const std::string& filename) = 0;
    virtual bool del(const std::string& filename) = 0;
    virtual void displayStructure() = 0; // Muestra el estado interno (FAT, inodos, etc.)
    virtual void displayDiskUsage() = 0; // Muestra qué bloques están ocupados y por qué archivo.
};

// --- Implementación del Sistema de Archivos FAT ---
class FATFileSystem : public FileSystem {
private:
    // La tabla FAT: un vector de enteros donde cada índice corresponde a un bloque del disco.
    std::vector<int> fat_table;
    // Directorio: mapea nombres de archivo a su bloque de inicio.
    std::map<std::string, int> directory;

public:
    FATFileSystem(std::shared_ptr<Disk> d);

    bool create(const std::string& filename, const std::string& content) override;
    std::string read(const std::string& filename) override;
    bool del(const std::string& filename) override;
    void displayStructure() override;
    void displayDiskUsage() override;
};

// --- Implementación del Sistema de Archivos Ext2 ---
class Ext2FileSystem : public FileSystem {
private:
    // Tabla de inodos: un vector donde cada inodo describe un archivo.
    std::vector<Inode> inode_table;
    // Directorio: mapea nombres de archivo a su número de inodo.
    std::map<std::string, int> directory;
    
    // Función auxiliar para encontrar un inodo libre.
    int findFreeInode() {
        for (size_t i = 1; i < inode_table.size(); ++i) { // Inodo 0 se reserva.
            if (!inode_table[i].is_used) {
                return i;
            }
        }
        return -1;
    }

public:
    Ext2FileSystem(std::shared_ptr<Disk> d);

    bool create(const std::string& filename, const std::string& content) override;
    std::string read(const std::string& filename) override;
    bool del(const std::string& filename) override;
    void displayStructure() override;
    void displayDiskUsage() override;
};

void run_simulator();

#endif // FILESYSTEMSIMULATOR_H
