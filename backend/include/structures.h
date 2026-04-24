#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <time.h>
#include <cstring>

#pragma pack(1)

// Partition
typedef struct {
    char part_status;
    char part_type;
    char part_fit;
    int part_start;
    int part_s;
    char part_name[16];
    int part_correlative;
    char part_id[4];
} Partition;

// Master Boot Record
typedef struct {
    int mbr_tamano;
    time_t mbr_fecha_creacion;
    int mbr_dsk_signature;
    char dsk_fit;
    Partition mbr_partitions[4];
} MBR;

// Extended Boot Record
typedef struct {
    char part_mount;
    char part_fit;
    int part_start;
    int part_s;
    int part_next;
    char part_name[16];
} EBR;

// Superblock EXT2/EXT3
typedef struct {
    int s_filesystem_type;      // 2 para EXT2, 3 para EXT3
    int s_inodes_count;
    int s_blocks_count;
    int s_free_blocks_count;
    int s_free_inodes_count;
    time_t s_mtime;
    time_t s_umtime;
    int s_mnt_count;
    int s_magic;
    int s_inode_s;
    int s_block_s;
    int s_firts_ino;
    int s_first_blo;
    int s_bm_inode_start;
    int s_bm_block_start;
    int s_inode_start;
    int s_block_start;
    int s_journal_start;        // EXT3
} Superblock;

// Inodo
typedef struct {
    int i_uid;
    int i_gid;
    int i_s;
    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;
    int i_block[15];
    char i_type;
    char i_perm[3];
} Inodo;

// Contenido de bloque (archivo/carpeta)
typedef struct {
    char b_name[12];
    int b_inodo;
} ContentBlock;

// Bloque de carpeta
typedef struct {
    ContentBlock b_content[4];
} BlockFolder;

// Bloque de archivo
typedef struct {
    char b_content[64];
} BlockFile;

// Bloque de apuntadores
typedef struct {
    int b_pointers[16];
} BlockPointers;

// Bitmap byte
typedef struct {
    unsigned char bitmap;
} BitmapByte;

// Registro de usuario/grupo
typedef struct {
    int id;
    char type;
    char group[11];
    char user[11];
    char pass[11];
} UserRecord;

// Information - Contenido del Journal
typedef struct {
    char i_operation[10];   // Tipo de operación (mkdir, mkfile, remove, etc.)
    char i_path[32];        // Ruta donde se realizó la operación
    char i_content[64];     // Contenido del archivo/información adicional
    float i_date;           // Timestamp de la operación
} Information;

// Journal - Bitácora de transacciones del sistema de archivos
typedef struct {
    int j_count;                    // Contador de transacciones registradas
    Information j_content[50];      // Máximo 50 entradas en el journal
} Journal;

#pragma pack()

#endif // STRUCTURES_H
