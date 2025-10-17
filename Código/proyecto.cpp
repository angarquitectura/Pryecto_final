#include "proyecto.h"
#include <cstring>

// --- Implementación de FATFileSystem ---

// Constructor de FAT: inicializa la tabla FAT.
FATFileSystem::FATFileSystem(std::shared_ptr<Disk> d) : FileSystem(d) {
    fat_table.assign(DISK_BLOCKS, FAT_FREE);
}

bool FATFileSystem::create(const std::string& filename, const std::string& content) {
    if (directory.count(filename)) {
        std::cout << "Error: El archivo ya existe." << std::endl;
        return false;
    }

    // Calcula cuántos bloques se necesitan.
    int blocks_needed = (content.length() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed == 0) blocks_needed = 1; // Un archivo vacío necesita al menos un bloque.

    // Busca y recolecta suficientes bloques libres.
    std::vector<int> free_blocks;
    for (int i = 0; i < DISK_BLOCKS && free_blocks.size() < blocks_needed; ++i) {
        if (disk->blocks[i].is_free && fat_table[i] == FAT_FREE) {
            free_blocks.push_back(i);
        }
    }

    if (free_blocks.size() < blocks_needed) {
        std::cout << "Error: No hay suficiente espacio en el disco." << std::endl;
        return false;
    }

    // Enlaza los bloques en la tabla FAT.
    for (size_t i = 0; i < free_blocks.size() - 1; ++i) {
        fat_table[free_blocks[i]] = free_blocks[i + 1];
    }
    fat_table[free_blocks.back()] = FAT_EOF; // El último bloque apunta a Fin de Archivo.

    // Escribe el contenido en los bloques del disco.
    for (size_t i = 0; i < free_blocks.size(); ++i) {
        int block_idx = free_blocks[i];
        // Extrae el trozo de contenido para este bloque.
        std::string data_chunk = content.substr(i * BLOCK_SIZE, BLOCK_SIZE);
        disk->writeBlock(block_idx, data_chunk);
    }

    // Registra el archivo en el directorio.
    directory[filename] = free_blocks[0];
    std::cout << "Archivo '" << filename << "' creado exitosamente." << std::endl;
    return true;
}

std::string FATFileSystem::read(const std::string& filename) {
    if (!directory.count(filename)) {
        return "Error: El archivo no existe.";
    }

    std::string content = "";
    int current_block = directory[filename];

    // Sigue la cadena en la tabla FAT hasta llegar al final.
    while (current_block != FAT_EOF) {
        content += disk->readBlock(current_block);
        current_block = fat_table[current_block];
    }
    
    // El contenido puede tener relleno nulo al final, lo eliminamos.
    return content.c_str(); 
}

bool FATFileSystem::del(const std::string& filename) {
    if (!directory.count(filename)) {
        std::cout << "Error: El archivo no existe." << std::endl;
        return false;
    }

    int current_block = directory[filename];

    // Sigue la cadena en la tabla FAT, liberando cada bloque.
    while (current_block != FAT_EOF) {
        int next_block = fat_table[current_block];
        disk->freeBlock(current_block);
        fat_table[current_block] = FAT_FREE;
        current_block = next_block;
    }

    directory.erase(filename);
    std::cout << "Archivo '" << filename << "' eliminado exitosamente." << std::endl;
    return true;
}

void FATFileSystem::displayStructure() {
    std::cout << "\n--- Estructura del Directorio FAT ---\n";
    for (const auto& pair : directory) {
        std::cout << "  " << pair.first << " -> Inicia en bloque " << pair.second << std::endl;
    }

    std::cout << "\n--- Tabla FAT (primeros 32 bloques) ---\n";
    for(int i = 0; i < 32 && i < DISK_BLOCKS; ++i) {
        std::cout << "  Bloque[" << i << "]: " << fat_table[i] << std::endl;
    }
}

void FATFileSystem::displayDiskUsage() {
    std::cout << "\n--- Uso del Disco (FAT) ---\n";
    std::map<int, std::string> block_to_file;
    for (const auto& pair : directory) {
        int current_block = pair.second;
        while (current_block != FAT_EOF) {
            block_to_file[current_block] = pair.first;
            current_block = fat_table[current_block];
        }
    }

    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (!disk->blocks[i].is_free) {
            std::cout << "Bloque " << std::setw(3) << i << ": Ocupado por '" << block_to_file[i] << "'" << std::endl;
        }
    }
}


// --- Implementación de Ext2FileSystem ---

// Constructor de Ext2: inicializa la tabla de inodos.
Ext2FileSystem::Ext2FileSystem(std::shared_ptr<Disk> d) : FileSystem(d) {
    inode_table.resize(32); // Limitamos a 32 inodos para esta simulación.
}

bool Ext2FileSystem::create(const std::string& filename, const std::string& content) {
    if (directory.count(filename)) {
        std::cout << "Error: El archivo ya existe." << std::endl;
        return false;
    }

    int inode_idx = findFreeInode();
    if (inode_idx == -1) {
        std::cout << "Error: No hay inodos libres." << std::endl;
        return false;
    }

    int blocks_needed = (content.length() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed == 0) blocks_needed = 1;

    std::vector<int> free_blocks;
    for (int i = 0; i < DISK_BLOCKS && free_blocks.size() < blocks_needed; ++i) {
        if (disk->blocks[i].is_free) {
            free_blocks.push_back(i);
        }
    }

    if (free_blocks.size() < blocks_needed) {
        std::cout << "Error: No hay suficiente espacio en el disco." << std::endl;
        return false;
    }

    // Asigna los bloques al inodo.
    Inode& inode = inode_table[inode_idx];
    inode.is_used = true;
    inode.size = content.length();
    int blocks_assigned = 0;

    // 1. Llenar punteros directos.
    for (int i = 0; i < NUM_DIRECT_POINTERS && blocks_assigned < blocks_needed; ++i) {
        inode.direct_pointers[i] = free_blocks[blocks_assigned];
        blocks_assigned++;
    }

    // 2. Llenar puntero indirecto simple (si es necesario).
    if (blocks_assigned < blocks_needed) {
        int indirect_block_idx = disk->findFreeBlock();
        if(indirect_block_idx == -1) { /* error handling */ return false; }
        disk->blocks[indirect_block_idx].is_free = false; // Reservamos el bloque para punteros.
        inode.single_indirect = indirect_block_idx;
        
        std::string pointer_data;
        for (int i = blocks_assigned; i < blocks_needed; ++i) {
            int block_ptr = free_blocks[i];
            pointer_data.append(reinterpret_cast<const char*>(&block_ptr), sizeof(int));
        }
        disk->writeBlock(indirect_block_idx, pointer_data);
    }

    // Escribe el contenido del archivo en los bloques de datos.
    for (size_t i = 0; i < free_blocks.size(); ++i) {
        std::string data_chunk = content.substr(i * BLOCK_SIZE, BLOCK_SIZE);
        disk->writeBlock(free_blocks[i], data_chunk);
    }
    
    directory[filename] = inode_idx;
    std::cout << "Archivo '" << filename << "' creado exitosamente." << std::endl;
    return true;
}

std::string Ext2FileSystem::read(const std::string& filename) {
    if (!directory.count(filename)) {
        return "Error: El archivo no existe.";
    }

    int inode_idx = directory[filename];
    const Inode& inode = inode_table[inode_idx];
    std::string content = "";

    // 1. Leer desde punteros directos.
    for (int block_ptr : inode.direct_pointers) {
        if (block_ptr != -1) {
            content += disk->readBlock(block_ptr);
        }
    }

    // 2. Leer desde puntero indirecto simple.
    if (inode.single_indirect != -1) {
        std::string pointer_block_content = disk->readBlock(inode.single_indirect);
        for(size_t i = 0; i < pointer_block_content.length(); i += sizeof(int)) {
            int block_ptr;
            std::memcpy(&block_ptr, pointer_block_content.c_str() + i, sizeof(int));
            content += disk->readBlock(block_ptr);
        }
    }

    // Recorta el contenido al tamaño real del archivo.
    return content.substr(0, inode.size);
}

bool Ext2FileSystem::del(const std::string& filename) {
    if (!directory.count(filename)) {
        std::cout << "Error: El archivo no existe." << std::endl;
        return false;
    }

    int inode_idx = directory[filename];
    Inode& inode = inode_table[inode_idx];

    // 1. Liberar bloques de punteros directos.
    for (int& block_ptr : inode.direct_pointers) {
        if (block_ptr != -1) {
            disk->freeBlock(block_ptr);
            block_ptr = -1;
        }
    }

    // 2. Liberar bloques de puntero indirecto.
    if (inode.single_indirect != -1) {
        std::string pointer_block_content = disk->readBlock(inode.single_indirect);
        for(size_t i = 0; i < pointer_block_content.length(); i += sizeof(int)) {
            int block_ptr;
            std::memcpy(&block_ptr, pointer_block_content.c_str() + i, sizeof(int));
            if(block_ptr >= 0) disk->freeBlock(block_ptr);
        }
        // Liberar el bloque de punteros en sí.
        disk->freeBlock(inode.single_indirect);
        inode.single_indirect = -1;
    }
    
    inode.is_used = false;
    inode.size = 0;
    directory.erase(filename);
    std::cout << "Archivo '" << filename << "' eliminado exitosamente." << std::endl;
    return true;
}

void Ext2FileSystem::displayStructure() {
    std::cout << "\n--- Estructura del Directorio Ext2 ---\n";
    for (const auto& pair : directory) {
        std::cout << "  " << pair.first << " -> Inodo " << pair.second << std::endl;
    }

    std::cout << "\n--- Tabla de Inodos (primeros 5 usados) ---\n";
    int count = 0;
    for (size_t i = 1; i < inode_table.size() && count < 5; ++i) {
        if (inode_table[i].is_used) {
            std::cout << "  Inodo[" << i << "]: Usado, Tamaño: " << inode_table[i].size << ", Punteros Directos: [ ";
            for(int ptr : inode_table[i].direct_pointers) {
                if(ptr != -1) std::cout << ptr << " ";
            }
            std::cout << "], Indirecto: " << inode_table[i].single_indirect << std::endl;
            count++;
        }
    }
}

void Ext2FileSystem::displayDiskUsage() {
    std::cout << "\n--- Uso del Disco (Ext2) ---\n";
    for (int i = 0; i < DISK_BLOCKS; ++i) {
        if (!disk->blocks[i].is_free) {
            std::cout << "Bloque " << std::setw(3) << i << ": Ocupado" << std::endl;
        }
    }
}

// --- Función Principal ---
// Gestiona el menú y la interacción con el usuario.
void run_simulator() {
    auto disk = std::make_shared<Disk>();
    std::unique_ptr<FileSystem> fs;

    std::cout << "--- Simulador de Sistema de Archivos ---\n";
    std::cout << "Seleccione el sistema de archivos a simular:\n";
    std::cout << "1. FAT\n";
    std::cout << "2. Ext2\n";
    int choice;
    std::cin >> choice;

    if (choice == 1) {
        fs = std::make_unique<FATFileSystem>(disk);
        std::cout << "Sistema de archivos FAT inicializado.\n";
    } else if (choice == 2) {
        fs = std::make_unique<Ext2FileSystem>(disk);
        std::cout << "Sistema de archivos Ext2 inicializado.\n";
    } else {
        std::cout << "Opción inválida." << std::endl;
        return;
    }

    std::string command, filename, content;
    while (true) {
        std::cout << "\n> Ingrese comando (create, read, delete, display, disk, exit): ";
        std::cin >> command;

        if (command == "create") {
            std::cout << "  Nombre del archivo: ";
            std::cin >> filename;
            std::cout << "  Contenido (sin espacios, use '_' en su lugar): ";
            std::cin >> content;
            fs->create(filename, content);
        } else if (command == "read") {
            std::cout << "  Nombre del archivo: ";
            std::cin >> filename;
            std::cout << "  Contenido: " << fs->read(filename) << std::endl;
        } else if (command == "delete") {
            std::cout << "  Nombre del archivo: ";
            std::cin >> filename;
            fs->del(filename);
        } else if (command == "display") {
            fs->displayStructure();
        } else if (command == "disk") {
            fs->displayDiskUsage();
        } else if (command == "exit") {
            break;
        } else {
            std::cout << "Comando desconocido." << std::endl;
        }
    }
}

// int main() {
//     run_simulator();
//     return 0;
// }
