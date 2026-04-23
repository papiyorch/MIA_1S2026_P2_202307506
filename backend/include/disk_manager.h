#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <string>
#include <fstream>
#include <cstring>
#include <iostream>
#include "structures.h"

class DiskManager {
public:
    // Crea un archivo .mia (disco virtual)
    static bool createDisk(const std::string& path, int size, char fit = 'F');
    
    // Elimina un archivo de disco
    static bool removeDisk(const std::string& path);
    
    // Lee datos del disco
    static bool readFromDisk(const std::string& path, int offset, char* buffer, int size);
    
    // Escribe datos en el disco
    static bool writeToDisk(const std::string& path, int offset, const char* buffer, int size);
    
    // Lee el MBR del disco
    static bool readMBR(const std::string& path, MBR& mbr);
    
    // Escribe el MBR del disco
    static bool writeMBR(const std::string& path, const MBR& mbr);
    
    // Verifica si un archivo existe
    static bool fileExists(const std::string& path);

    // Obtiene el tamaño de un archivo en bytes
    static long getFileSize(const std::string& path);

    // Crea directorios necesarios para la ruta
    static bool createDirectories(const std::string& path);
    
    // Busca una partición por nombre
    static int findPartitionByName(const std::string& path, const std::string& name);
    
    // Obtiene espacio libre en disco
    static int getFreeSpace(const std::string& path, int diskSize);
    
    // Calcula donde colocar una partición (Best Fit, First Fit, Worst Fit)
    static int calculatePartitionStart(const std::string& path, int size, char fit, int diskSize);
    
    // Superblock operations
    static bool readSuperblock(const std::string& path, int partStart, Superblock& sb);
    static bool writeSuperblock(const std::string& path, int partStart, const Superblock& sb);
    
    // Inode operations
    static bool readInodo(const std::string& path, int partStart, int inodeNum, Inodo& ino);
    static bool writeInodo(const std::string& path, int partStart, int inodeNum, const Inodo& ino);
    static int allocateInode(const std::string& path, int partStart, Superblock& sb);
    static bool deallocateInode(const std::string& path, int partStart, int inodeNum, Superblock& sb);
    
    // Block operations
    static bool readBlock(const std::string& path, int partStart, int blockNum, char* buffer, int size);
    static bool writeBlock(const std::string& path, int partStart, int blockNum, const char* buffer, int size);
    static int allocateBlock(const std::string& path, int partStart, Superblock& sb);
    static bool deallocateBlock(const std::string& path, int partStart, int blockNum, Superblock& sb);
    
    // Bitmap operations
    static bool getBitmapBit(const std::string& path, int bitmapStart, int bitNum);
    static bool setBitmapBit(const std::string& path, int bitmapStart, int bitNum, bool value);
    
    // Journal operations (EXT3)
    static bool readJournal(const std::string& path, int partStart, Journal& journal);
    static bool writeJournal(const std::string& path, int partStart, const Journal& journal);
    static bool addJournalEntry(const std::string& path, int partStart, const Information& info);
    static bool clearJournal(const std::string& path, int partStart);
    
    // EXT2 Layout calculations 
    struct EXT2Layout {
        int num_inodes;           // Número de inodos calculados
        int num_blocks;           // Número de bloques (3 * num_inodes)
        int inode_bitmap_start;   // Offset del bitmap de inodos
        int block_bitmap_start;   // Offset del bitmap de bloques
        int inode_table_start;    // Offset de la tabla de inodos
        int block_table_start;    // Offset de la tabla de bloques
    };
    
    // EXT3 Layout calculations 
    struct EXT3Layout {
        int num_inodes;           // Número de inodos calculados
        int num_blocks;           // Número de bloques (3 * num_inodes)
        int journal_start;        // Offset del journal
        int inode_bitmap_start;   // Offset del bitmap de inodos
        int block_bitmap_start;   // Offset del bitmap de bloques
        int inode_table_start;    // Offset de la tabla de inodos
        int block_table_start;    // Offset de la tabla de bloques
    };
    
    // Calcula el layout EXT2 para una partición (sin journal)
    static EXT2Layout calculateEXT2Layout(int partitionSize);
    
    // Calcula el layout EXT3 para una partición (con journal)
    static EXT3Layout calculateEXT3Layout(int partitionSize);
};

#endif // DISK_MANAGER_H
