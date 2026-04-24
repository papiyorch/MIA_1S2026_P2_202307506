#include "disk_manager.h"
#include <filesystem>
#include <random>
#include <ctime>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

bool DiskManager::createDisk(const std::string& path, int size, char fit) {
    // Crear directorios si no existen
    if (!createDirectories(path)) {
        return false;
    }

    // Verificar que el tamaño sea positivo
    if (size <= sizeof(MBR)) {
        std::cerr << "Error: Tamaño de disco menor que MBR" << std::endl;
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo crear archivo: " << path << std::endl;
        return false;
    }

    // Crear MBR inicial
    MBR mbr;
    mbr.mbr_tamano = size;
    mbr.mbr_fecha_creacion = time(nullptr);
    
    // Generar número aleatorio único
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 2147483647);
    mbr.mbr_dsk_signature = dis(gen);
    
    mbr.dsk_fit = fit;

    // Inicializar particiones
    for (int i = 0; i < 4; i++) {
        mbr.mbr_partitions[i].part_status = '0';
        mbr.mbr_partitions[i].part_type = 'N';
        mbr.mbr_partitions[i].part_fit = 'N';
        mbr.mbr_partitions[i].part_start = -1;
        mbr.mbr_partitions[i].part_s = 0;
        mbr.mbr_partitions[i].part_correlative = -1;
        std::strcpy(mbr.mbr_partitions[i].part_name, "");
        std::strcpy(mbr.mbr_partitions[i].part_id, "");
    }

    // Escribir MBR al inicio
    file.seekp(0);
    file.write((char*)&mbr, sizeof(MBR));

    if (!file.good()) {
        file.close();
        std::cerr << "Error: No se pudo escribir MBR" << std::endl;
        return false;
    }

    // Llenar resto con ceros usando buffer de 1024 bytes
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    
    int bytesWritten = sizeof(MBR);
    while (bytesWritten < size) {
        int remaining = size - bytesWritten;
        int toWrite = (remaining < 1024) ? remaining : 1024;
        file.write(buffer, toWrite);
        bytesWritten += toWrite;
    }

    file.close();

    if (!file.good()) {
        std::cerr << "Error: No se pudo escribir completamente el archivo" << std::endl;
        return false;
    }

    std::cout << "Disco creado exitosamente: " << path << std::endl;
    return true;
}

bool DiskManager::removeDisk(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            std::cerr << "Error: Archivo no existe: " << path << std::endl;
            return false;
        }
        fs::remove(path);
        std::cout << "Disco eliminado: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error al eliminar disco: " << e.what() << std::endl;
        return false;
    }
}

bool DiskManager::readFromDisk(const std::string& path, int offset, char* buffer, int size) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir archivo: " << path << std::endl;
        return false;
    }

    file.seekg(offset);
    file.read(buffer, size);

    if (!file.good()) {
        std::cerr << "Error: No se pudo leer del disco" << std::endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool DiskManager::writeToDisk(const std::string& path, int offset, const char* buffer, int size) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir archivo: " << path << std::endl;
        return false;
    }

    file.seekp(offset);
    file.write(buffer, size);

    if (!file.good()) {
        std::cerr << "Error: No se pudo escribir en el disco" << std::endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool DiskManager::readMBR(const std::string& path, MBR& mbr) {
    return readFromDisk(path, 0, (char*)&mbr, sizeof(MBR));
}

bool DiskManager::writeMBR(const std::string& path, const MBR& mbr) {
    return writeToDisk(path, 0, (char*)&mbr, sizeof(MBR));
}

bool DiskManager::fileExists(const std::string& path) {
    return fs::exists(path);
}

long DiskManager::getFileSize(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return -1;
        }
        return fs::file_size(path);
    } catch (const std::exception& e) {
        std::cerr << "Error obteniendo tamaño: " << e.what() << std::endl;
        return -1;
    }
}

bool DiskManager::createDirectories(const std::string& path) {
    try {
        fs::path p(path);
        fs::path parent = p.parent_path();
        
        if (!parent.empty() && !fs::exists(parent)) {
            fs::create_directories(parent);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creando directorios: " << e.what() << std::endl;
        return false;
    }
}

int DiskManager::findPartitionByName(const std::string& path, const std::string& name) {
    MBR mbr;
    if (!readMBR(path, mbr)) {
        return -1;  // Error al leer
    }
    
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_name) == name && mbr.mbr_partitions[i].part_type != 'N') {
            return i;  // Encontrada en posición i del MBR
        }
    }
    return -1;  // No encontrada
}

int DiskManager::getFreeSpace(const std::string& path, int diskSize) {
    MBR mbr;
    if (!readMBR(path, mbr)) {
        return -1;
    }
    
    int usedSpace = sizeof(MBR);  // El MBR ocupa espacio
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type != 'N') {
            usedSpace += mbr.mbr_partitions[i].part_s;
        }
    }
    
    return diskSize - usedSpace;
}

int DiskManager::calculatePartitionStart(const std::string& path, int size, char fit, int diskSize) {
    MBR mbr;
    if (!readMBR(path, mbr)) {
        return -1;
    }
    
    if (fit == 'F') {  // First Fit
        int currentPos = sizeof(MBR);
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type != 'N') {
                currentPos = mbr.mbr_partitions[i].part_start + mbr.mbr_partitions[i].part_s;
            }
        }
        if (currentPos + size <= diskSize) {
            return currentPos;
        }
        return -1;  // No hay espacio
    }
    
    // Best Fit y Worst Fit: buscar mejor/peor hueco
    int bestStart = -1, bestSize = diskSize;
    int worstStart = -1, worstSize = 0;
    
    int currentPos = sizeof(MBR);
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type != 'N') {
            int gapSize = mbr.mbr_partitions[i].part_start - currentPos;
            if (gapSize >= size) {
                if (fit == 'B' && gapSize < bestSize) {  // Best Fit
                    bestStart = currentPos;
                    bestSize = gapSize;
                } else if (fit == 'W' && gapSize > worstSize) {  // Worst Fit
                    worstStart = currentPos;
                    worstSize = gapSize;
                }
            }
            currentPos = mbr.mbr_partitions[i].part_start + mbr.mbr_partitions[i].part_s;
        }
    }
    
    // Último espacio
    int gapSize = diskSize - currentPos;
    if (gapSize >= size) {
        if (fit == 'B' && gapSize < bestSize) {
            bestStart = currentPos;
            bestSize = gapSize;
        } else if (fit == 'W' && gapSize > worstSize) {
            worstStart = currentPos;
            worstSize = gapSize;
        }
    }
    
    return (fit == 'B') ? bestStart : worstStart;
}

bool DiskManager::readSuperblock(const std::string& path, int partStart, Superblock& sb) {
    return readFromDisk(path, partStart, (char*)&sb, sizeof(Superblock));
}

bool DiskManager::writeSuperblock(const std::string& path, int partStart, const Superblock& sb) {
    return writeToDisk(path, partStart, (char*)&sb, sizeof(Superblock));
}

bool DiskManager::readInodo(const std::string& path, int partStart, int inodeNum, Inodo& ino) {
    Superblock sb;
    if (!readSuperblock(path, partStart, sb)) return false;
    int offset = sb.s_inode_start + (inodeNum * sizeof(Inodo));
    return readFromDisk(path, offset, (char*)&ino, sizeof(Inodo));
}

bool DiskManager::writeInodo(const std::string& path, int partStart, int inodeNum, const Inodo& ino) {
    Superblock sb;
    if (!readSuperblock(path, partStart, sb)) return false;
    int offset = sb.s_inode_start + (inodeNum * sizeof(Inodo));
    return writeToDisk(path, offset, (char*)&ino, sizeof(Inodo));
}

int DiskManager::allocateInode(const std::string& path, int partStart, Superblock& sb) {
    if (sb.s_free_inodes_count <= 0) return -1;
    
    // Lectura optimizada del bitmap en chunks
    int bitmapBytes = (sb.s_inodes_count + 7) / 8;
    int chunkSize = 4096;  // Leer 4KB a la vez
    unsigned char buffer[4096];
    
    for (int byteOffset = 0; byteOffset < bitmapBytes; byteOffset += chunkSize) {
        int toRead = std::min(chunkSize, bitmapBytes - byteOffset);
        if (!readFromDisk(path, sb.s_bm_inode_start + byteOffset, (char*)buffer, toRead)) {
            continue;
        }
        
        // Buscar byte libre o semilibre en este chunk
        for (int b = 0; b < toRead; b++) {
            if (buffer[b] != 0xFF) {  // Hay al menos un bit libre
                // Buscar el primer bit libre en este byte - usando directamente el buffer
                for (int bit = 0; bit < 8; bit++) {
                    int inodeNum = (byteOffset + b) * 8 + bit;
                    if (inodeNum >= sb.s_inodes_count) break;
                    if (inodeNum > 0) {
                        // Verificar directamente en buffer sin re-leer disco
                        bool isFree = (buffer[b] & (1 << bit)) == 0;
                        if (isFree) {
                            // Marcar como usado en buffer
                            buffer[b] |= (1 << bit);
                            // Escribir byte actualizado al disco
                            writeToDisk(path, sb.s_bm_inode_start + byteOffset + b, (char*)&buffer[b], 1);
                            sb.s_free_inodes_count--;
                            return inodeNum;
                        }
                    }
                }
            }
        }
    }
    return -1;
}

bool DiskManager::deallocateInode(const std::string& path, int partStart, int inodeNum, Superblock& sb) {
    if (inodeNum < 0 || inodeNum >= sb.s_inodes_count) return false;
    if (!setBitmapBit(path, sb.s_bm_inode_start, inodeNum, false)) return false;
    sb.s_free_inodes_count++;
    return writeSuperblock(path, partStart, sb);
}

bool DiskManager::readBlock(const std::string& path, int partStart, int blockNum, char* buffer, int size) {
    Superblock sb;
    if (!readSuperblock(path, partStart, sb)) return false;
    int offset = sb.s_block_start + (blockNum * sb.s_block_s);
    return readFromDisk(path, offset, buffer, size);
}

bool DiskManager::writeBlock(const std::string& path, int partStart, int blockNum, const char* buffer, int size) {
    Superblock sb;
    if (!readSuperblock(path, partStart, sb)) return false;
    int offset = sb.s_block_start + (blockNum * sb.s_block_s);
    return writeToDisk(path, offset, buffer, size);
}

int DiskManager::allocateBlock(const std::string& path, int partStart, Superblock& sb) {
    if (sb.s_free_blocks_count <= 0) return -1;
    
    // Lectura optimizada del bitmap en chunks
    int bitmapBytes = (sb.s_blocks_count + 7) / 8;
    int chunkSize = 4096;  // Leer 4KB a la vez
    unsigned char buffer[4096];
    
    for (int byteOffset = 0; byteOffset < bitmapBytes; byteOffset += chunkSize) {
        int toRead = std::min(chunkSize, bitmapBytes - byteOffset);
        if (!readFromDisk(path, sb.s_bm_block_start + byteOffset, (char*)buffer, toRead)) {
            continue;
        }
        
        // Buscar byte libre o semilibre en este chunk
        for (int b = 0; b < toRead; b++) {
            if (buffer[b] != 0xFF) {  // Hay al menos un bit libre
                // Buscar el primer bit libre en este byte - usando directamente el buffer
                for (int bit = 0; bit < 8; bit++) {
                    int blockNum = (byteOffset + b) * 8 + bit;
                    if (blockNum >= sb.s_blocks_count) break;
                    // Verificar directamente en buffer sin re-leer disco
                    bool isFree = (buffer[b] & (1 << bit)) == 0;
                    if (isFree) {
                        // Marcar como usado en buffer
                        buffer[b] |= (1 << bit);
                        // Escribir byte actualizado al disco
                        writeToDisk(path, sb.s_bm_block_start + byteOffset + b, (char*)&buffer[b], 1);
                        sb.s_free_blocks_count--;
                        return blockNum;
                    }
                }
            }
        }
    }
    return -1;
}

bool DiskManager::deallocateBlock(const std::string& path, int partStart, int blockNum, Superblock& sb) {
    if (blockNum < 0 || blockNum >= sb.s_blocks_count) return false;
    if (!setBitmapBit(path, sb.s_bm_block_start, blockNum, false)) return false;
    sb.s_free_blocks_count++;
    return writeSuperblock(path, partStart, sb);
}

bool DiskManager::getBitmapBit(const std::string& path, int bitmapStart, int bitNum) {
    int bytePos = bitNum / 8;
    int bitPos = bitNum % 8;
    unsigned char byte;
    if (!readFromDisk(path, bitmapStart + bytePos, (char*)&byte, 1)) return false;
    return (byte & (1 << bitPos)) != 0;
}

bool DiskManager::setBitmapBit(const std::string& path, int bitmapStart, int bitNum, bool value) {
    int bytePos = bitNum / 8;
    int bitPos = bitNum % 8;
    unsigned char byte;
    if (!readFromDisk(path, bitmapStart + bytePos, (char*)&byte, 1)) return false;
    
    if (value) {
        byte |= (1 << bitPos);
    } else {
        byte &= ~(1 << bitPos);
    }
    
    return writeToDisk(path, bitmapStart + bytePos, (char*)&byte, 1);
}

// Journal operations (EXT3)
bool DiskManager::readJournal(const std::string& path, int partStart, Journal& journal) {
    return readFromDisk(path, partStart + sizeof(Superblock), (char*)&journal, sizeof(Journal));
}

bool DiskManager::writeJournal(const std::string& path, int partStart, const Journal& journal) {
    return writeToDisk(path, partStart + sizeof(Superblock), (const char*)&journal, sizeof(Journal));
}

bool DiskManager::addJournalEntry(const std::string& path, int partStart, const Information& info) {
    Journal journal;
    
    // Leer el journal actual
    if (!readJournal(path, partStart, journal)) {
        std::cerr << "Error: No se pudo leer el journal" << std::endl;
        return false;
    }
    
    // Verificar que no esté lleno (máximo 50 entradas)
    if (journal.j_count >= 50) {
        std::cerr << "Error: Journal lleno (máximo 50 entradas)" << std::endl;
        return false;
    }
    
    // Agregar la nueva entrada
    journal.j_content[journal.j_count] = info;
    journal.j_count++;
    
    // Escribir el journal actualizado
    return writeJournal(path, partStart, journal);
}

bool DiskManager::clearJournal(const std::string& path, int partStart) {
    Journal journal;
    journal.j_count = 0;
    
    // Limpiar todas las entradas
    for (int i = 0; i < 50; i++) {
        std::memset(journal.j_content[i].i_operation, 0, sizeof(journal.j_content[i].i_operation));
        std::memset(journal.j_content[i].i_path, 0, sizeof(journal.j_content[i].i_path));
        std::memset(journal.j_content[i].i_content, 0, sizeof(journal.j_content[i].i_content));
        journal.j_content[i].i_date = 0;
    }
    
    return writeJournal(path, partStart, journal);
}

// EXT2 Layout calculation (SIN journal)
DiskManager::EXT2Layout DiskManager::calculateEXT2Layout(int partitionSize) {
    EXT2Layout layout;
    
    int superblock_size = sizeof(Superblock);
    int inodo_size = sizeof(Inodo);
    int block_size = sizeof(BlockFile);
    
    // Calcular espacio disponible después del superblock
    int available_space = partitionSize - superblock_size;
    
    // Calcular el denominador para despejar n
    double denominator = 0.5 + (double)inodo_size + 3.0 * (double)block_size;
    
    // Calcular n (número de inodos)
    double num_inodes_double = available_space / denominator;
    layout.num_inodes = (int)floor(num_inodes_double);  // floor explícito
    
    // Validar un mínimo
    if (layout.num_inodes < 1) {
        layout.num_inodes = 1;
    }
    
    // Número de bloques es el triple
    layout.num_blocks = layout.num_inodes * 3;
    
    // Calcular offsets 
    layout.inode_bitmap_start = superblock_size;
    layout.block_bitmap_start = layout.inode_bitmap_start + (layout.num_inodes / 8) + 1;
    layout.inode_table_start = layout.block_bitmap_start + (layout.num_blocks / 8) + 1;
    layout.block_table_start = layout.inode_table_start + (layout.num_inodes * inodo_size);
    
    return layout;
}

// EXT3 Layout calculation (CON journal)
DiskManager::EXT3Layout DiskManager::calculateEXT3Layout(int partitionSize) {
    EXT3Layout layout;
    
    int superblock_size = sizeof(Superblock);
    int journal_size = sizeof(Journal);
    int inodo_size = sizeof(Inodo);
    int block_size = sizeof(BlockFile);
    
    // Calcular espacio disponible después del superblock
    int available_space = partitionSize - superblock_size;
    
    
    double denominator = (double)journal_size + 0.5 + (double)inodo_size + 3.0 * (double)block_size;
    
    // Calcular n (número de inodos)
    double num_inodes_double = available_space / denominator;
    layout.num_inodes = (int)floor(num_inodes_double);  // floor explícito
    
    // Validar un mínimo
    if (layout.num_inodes < 1) {
        layout.num_inodes = 1;
    }
    
    // Número de bloques es el triple
    layout.num_blocks = layout.num_inodes * 3;
    
    // Calcular offsets (relativos al inicio de la partición)
    layout.journal_start = superblock_size;
    layout.inode_bitmap_start = layout.journal_start + (layout.num_inodes * journal_size);
    layout.block_bitmap_start = layout.inode_bitmap_start + (layout.num_inodes / 8) + 1;
    layout.inode_table_start = layout.block_bitmap_start + (layout.num_blocks / 8) + 1;
    layout.block_table_start = layout.inode_table_start + (layout.num_inodes * inodo_size);
    
    return layout;
}
