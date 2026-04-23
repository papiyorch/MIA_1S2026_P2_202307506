#include "report_generator.h"
#include "disk_manager.h"
#include "command_parser.h"
#include <fstream>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <functional>
#include <set>

// Función auxiliar para escapar caracteres especiales en HTML
static std::string escapeHTMLSpecialChars(const std::string& input) {
    std::string output = input;
    size_t pos = 0;
    
    // Escapar & primero (debe ser primero para evitar doble escape)
    while ((pos = output.find("&", pos)) != std::string::npos) {
        output.replace(pos, 1, "&amp;");
        pos += 5;
    }
    
    // Escapar <
    pos = 0;
    while ((pos = output.find("<", pos)) != std::string::npos) {
        output.replace(pos, 1, "&lt;");
        pos += 4;
    }
    
    // Escapar >
    pos = 0;
    while ((pos = output.find(">", pos)) != std::string::npos) {
        output.replace(pos, 1, "&gt;");
        pos += 4;
    }
    
    // Escapar "
    pos = 0;
    while ((pos = output.find("\"", pos)) != std::string::npos) {
        output.replace(pos, 1, "&quot;");
        pos += 6;
    }
    
    return output;
}

std::string ReportGenerator::generateReport(const std::string& repName, const std::string& repPath,
                                           const std::string& partitionId, const std::string& diskPath,
                                           const MBR& mbr, const Partition* part,
                                           const std::map<std::string, std::string>& params) {
    
    // Crear carpeta si no existe
    std::string folderPath = repPath;
    size_t lastSlash = repPath.find_last_of("/");
    if (lastSlash != std::string::npos) {
        folderPath = repPath.substr(0, lastSlash);
        if (!folderPath.empty()) {
            system(("mkdir -p \"" + folderPath + "\" 2>/dev/null").c_str());
        }
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    std::string result = "";
    
    if (repName == "mbr") {
        result = generateMBR(repPath, mbr);
    } else if (repName == "sb") {
        result = generateSB(repPath, sb);
    } else if (repName == "inode") {
        result = generateInode(diskPath, part->part_start, repPath, sb);
    } else if (repName == "block") {
        result = generateBlock(diskPath, part->part_start, repPath, sb);
    } else if (repName == "bm_inode") {
        result = generateBmInode(diskPath, repPath, sb);
    } else if (repName == "bm_block") {
        result = generateBmBloc(diskPath, repPath, sb);
    } else if (repName == "tree") {
        result = generateTree(diskPath, part->part_start, repPath, sb);
    } else if (repName == "file") {
        result = generateFile(diskPath, part->part_start, repPath, sb, params);
    } else if (repName == "ls") {
        result = generateLS(diskPath, part->part_start, repPath, sb, params);
    } else if (repName == "disk") {
        result = generateDisk(diskPath, repPath, mbr);
    } else if (repName == "ebr") {
        result = generateEBR(diskPath, repPath, part);
    }
    
    return result;
}

std::string ReportGenerator::generateMBR(const std::string& repPath, const MBR& mbr) {
    std::string graphvizContent = "digraph MBR {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  MBR [label=<\n";
    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
    
    // Encabezado del MBR
    graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">REPORTE DE MBR</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_tamano</TD><TD>" + std::to_string(mbr.mbr_tamano) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_fecha_creacion</TD><TD>" + std::string(__DATE__) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_dsk_signature</TD><TD>" + std::to_string(mbr.mbr_dsk_signature) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD COLSPAN=\"8\"></TD></TR>\n";
    
    // Particiones Primarias
    graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#A020F0\"><B><FONT COLOR=\"white\">Particiones Primarias</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
    
    bool hasPrimary = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'P') {
            hasPrimary = true;
            graphvizContent += "      <TR>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
            graphvizContent += "</TR>\n";
        }
    }
    
    if (!hasPrimary) {
        graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
    }
    
    // Particiones Extendidas
    graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">Particiones Extendidas</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
    
    bool hasExtended = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'E') {
            hasExtended = true;
            graphvizContent += "      <TR>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
            graphvizContent += "</TR>\n";
        }
    }
    
    if (!hasExtended) {
        graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
    }
    
    // Particiones Lógicas
    graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Particiones Lógicas</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
    
    bool hasLogical = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'L') {
            hasLogical = true;
            graphvizContent += "      <TR>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
            graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
            graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
            graphvizContent += "</TR>\n";
        }
    }
    
    if (!hasLogical) {
        graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
    }
    
    graphvizContent += "    </TABLE>\n";
    graphvizContent += "  >];\n";
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateSB(const std::string& repPath, const Superblock& sb) {
    std::string graphvizContent = "digraph Superblock {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  SB [label=<\n";
    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
    
    // Encabezado
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Reporte de SUPERBLOQUE</FONT></B></TD></TR>\n";
    
    // Fila de separador
    graphvizContent += "      <TR><TD COLSPAN=\"2\"></TD></TR>\n";
    
    // Información General del Filesystem
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Información General</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>s_filesystem_type</TD><TD>" + std::to_string(sb.s_filesystem_type) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_inodes_count</TD><TD>" + std::to_string(sb.s_inodes_count) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>s_blocks_count</TD><TD>" + std::to_string(sb.s_blocks_count) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_free_inodes_count</TD><TD>" + std::to_string(sb.s_free_inodes_count) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>s_free_blocks_count</TD><TD>" + std::to_string(sb.s_free_blocks_count) + "</TD></TR>\n";
    
    // Información de Fechas y Montajes
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Fechas y Montajes</FONT></B></TD></TR>\n";
    std::string mtimeStr = std::string(ctime(&sb.s_mtime));
    std::string umtimeStr = std::string(ctime(&sb.s_umtime));
    if (!mtimeStr.empty() && mtimeStr.back() == '\n') mtimeStr.pop_back();
    if (!umtimeStr.empty() && umtimeStr.back() == '\n') umtimeStr.pop_back();
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_mtime (fecha creación)</TD><TD>" + mtimeStr + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>s_umtime (último montaje)</TD><TD>" + umtimeStr + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_mnt_count</TD><TD>" + std::to_string(sb.s_mnt_count) + "</TD></TR>\n";
    
    // Información de Tamaños de Estructuras
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Tamaños de Estructuras</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>s_inode_s (tamaño inodo)</TD><TD>" + std::to_string(sb.s_inode_s) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_block_s (tamaño bloque)</TD><TD>" + std::to_string(sb.s_block_s) + "</TD></TR>\n";
    
    // Información de Primeros Inodos/Bloques
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Primeros Inodos/Bloques</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>s_firts_ino</TD><TD>" + std::to_string(sb.s_firts_ino) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_first_blo</TD><TD>" + std::to_string(sb.s_first_blo) + "</TD></TR>\n";
    
    // Información de Posiciones en Disco
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Posiciones en Disco</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>s_bm_inode_start</TD><TD>" + std::to_string(sb.s_bm_inode_start) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_bm_block_start</TD><TD>" + std::to_string(sb.s_bm_block_start) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>s_inode_start</TD><TD>" + std::to_string(sb.s_inode_start) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_block_start</TD><TD>" + std::to_string(sb.s_block_start) + "</TD></TR>\n";
    
    // Información de Identificación
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Identificación</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>s_magic</TD><TD>0x" + std::to_string(sb.s_magic) + "</TD></TR>\n";
    
    graphvizContent += "    </TABLE>\n";
    graphvizContent += "  >];\n";
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateInode(const std::string& diskPath, int partStart,
                                          const std::string& repPath, const Superblock& sb) {
    std::string graphvizContent = "digraph Inodes {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    
    int usedInodeCount = 0;
    
    // Iterar sobre todos los inodos del sistema
    for (int i = 0; i < sb.s_inodes_count; i++) {
        Inodo ino;
        if (DiskManager::readInodo(diskPath, partStart, i, ino)) {
            // Solo mostrar si el inodo está siendo utilizado
            if (ino.i_type >= 0) {
                std::string type = (ino.i_type == 0) ? "Dir" : ((ino.i_type == 1) ? "File" : "Unknown");
                
                // Convertir timestamp a formato seguro sin caracteres especiales
                char atimeStr[64];
                struct tm* timeinfo = localtime(&ino.i_atime);
                strftime(atimeStr, sizeof(atimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                
                // Crear nodo para este inodo
                std::string nodeId = "inode_" + std::to_string(i);
                graphvizContent += "  " + nodeId + " [label=<\n";
                graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
                graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">Inodo " + std::to_string(i) + "</FONT></B></TD></TR>\n";
                graphvizContent += "      <TR><TD>i_uid</TD><TD>" + std::to_string(ino.i_uid) + "</TD></TR>\n";
                graphvizContent += "      <TR><TD>i_size</TD><TD>" + std::to_string(ino.i_s) + "</TD></TR>\n";
                graphvizContent += "      <TR><TD>i_atime</TD><TD>" + std::string(atimeStr) + "</TD></TR>\n";
                
                // Mostrar los bloques asociados
                bool hasBlocks = false;
                for (int j = 0; j < 15; j++) {
                    if (ino.i_block[j] != -1) {
                        graphvizContent += "      <TR><TD>i_block_" + std::to_string(j+1) + "</TD><TD>" + std::to_string(ino.i_block[j]) + "</TD></TR>\n";
                        hasBlocks = true;
                    }
                }
                
                if (!hasBlocks) {
                    graphvizContent += "      <TR><TD>i_block_1</TD><TD>-1</TD></TR>\n";
                }
                
                graphvizContent += "      <TR><TD>i_perm</TD><TD>" + std::string(ino.i_perm) + "</TD></TR>\n";
                graphvizContent += "    </TABLE>\n";
                graphvizContent += "  >];\n";
                
                usedInodeCount++;
            }
        }
    }
    
    // Si no hay inodos utilizados, mostrar un mensaje
    if (usedInodeCount == 0) {
        graphvizContent += "  empty [label=\"No hay inodos utilizados\"];\n";
    }
    
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateBlock(const std::string& diskPath, int partStart,
                                          const std::string& repPath, const Superblock& sb) {
    std::string graphvizContent = "digraph Blocks {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    
    int blockCount = 0;
    
    // Iterar sobre todos los inodos para encontrar bloques utilizados
    for (int inodeIdx = 0; inodeIdx < sb.s_inodes_count; inodeIdx++) {
        Inodo ino;
        if (!DiskManager::readInodo(diskPath, partStart, inodeIdx, ino)) {
            continue;
        }
        
        // Solo procesar inodos utilizados
        if (ino.i_type < 0) {
            continue;
        }
        
        // Procesar cada bloque del inodo
        for (int blockIdx = 0; blockIdx < 15 && ino.i_block[blockIdx] != -1; blockIdx++) {
            int blockNum = ino.i_block[blockIdx];
            std::string nodeId = "block_" + std::to_string(blockNum);
            
            // Crear nodo para este bloque
            graphvizContent += "  " + nodeId + " [label=<\n";
            graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
            
            if (ino.i_type == 0) {
                // Es un directorio
                BlockFolder blockFolder;
                if (DiskManager::readBlock(diskPath, partStart, blockNum, 
                                          (char*)&blockFolder, sizeof(BlockFolder))) {
                    
                    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Bloque Carpeta " + std::to_string(blockNum) + "</FONT></B></TD></TR>\n";
                    graphvizContent += "      <TR><TD><B>b_name</B></TD><TD><B>b_inodo</B></TD></TR>\n";
                    
                    // Mostrar entradas del directorio
                    for (int j = 0; j < 4; j++) {
                        if (blockFolder.b_content[j].b_inodo != 0) {
                            graphvizContent += "      <TR><TD>" + std::string(blockFolder.b_content[j].b_name) + "</TD>";
                            graphvizContent += "<TD>" + std::to_string(blockFolder.b_content[j].b_inodo) + "</TD></TR>\n";
                        }
                    }
                }
                
            } else if (ino.i_type == 1) {
                // Es un archivo
                BlockFile blockFile;
                if (DiskManager::readBlock(diskPath, partStart, blockNum, 
                                          (char*)&blockFile, sizeof(BlockFile))) {
                    
                    graphvizContent += "      <TR><TD COLSPAN=\"1\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">Bloque Archivo " + std::to_string(blockNum) + "</FONT></B></TD></TR>\n";
                    
                    // Mostrar contenido del archivo
                    std::string content(blockFile.b_content);
                    if (content.length() > 64) {
                        content = content.substr(0, 64) + "...";
                    }
                    
                    // Escapar caracteres especiales para XML
                    std::string escapedContent = "";
                    for (char c : content) {
                        if (c == '<') escapedContent += "&lt;";
                        else if (c == '>') escapedContent += "&gt;";
                        else if (c == '&') escapedContent += "&amp;";
                        else if (c == '"') escapedContent += "&quot;";
                        else if (c == '\'') escapedContent += "&apos;";
                        else if (c == '\n') escapedContent += "\\n";
                        else if (c == '\t') escapedContent += "\\t";
                        else if (c >= 32 && c < 127) escapedContent += c;
                        else escapedContent += "?";
                    }
                    
                    graphvizContent += "      <TR><TD><FONT FACE=\"Courier\" SIZE=\"9\">" + escapedContent + "</FONT></TD></TR>\n";
                }
            }
            
            graphvizContent += "    </TABLE>\n";
            graphvizContent += "  >];\n";
            blockCount++;
        }
    }
    
    // Si no hay bloques utilizados
    if (blockCount == 0) {
        graphvizContent += "  empty [label=\"No hay bloques utilizados\"];\n";
    }
    
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateBmInode(const std::string& diskPath, const std::string& repPath,
                                            const Superblock& sb) {
    std::string textContent = "";
    
    // Calcular cantidad de bytes en el bitmap de inodos
    int bitmapBytes = (sb.s_inodes_count + 7) / 8;
    
    // Leer el bitmap desde el disco
    char* bitmapData = new char[bitmapBytes];
    if (!DiskManager::readFromDisk(diskPath, sb.s_bm_inode_start, bitmapData, bitmapBytes)) {
        delete[] bitmapData;
        return "Error: No se pudo leer el bitmap de inodos.";
    }
    
    // Generar contenido del reporte con 20 bits por línea
    int lineNumber = 1;
    int bitsPerLine = 20;
    int bitCount = 0;
    
    for (int byteIdx = 0; byteIdx < bitmapBytes; byteIdx++) {
        unsigned char currentByte = (unsigned char)bitmapData[byteIdx];
        
        // Procesar cada bit del byte
        for (int bitIdx = 0; bitIdx < 8 && bitCount < sb.s_inodes_count; bitIdx++) {
            // Extraer el bit (LSB primero)
            int bit = (currentByte >> bitIdx) & 1;
            
            // Si es el primer bit de la línea, agregar número de línea
            if (bitCount % bitsPerLine == 0) {
                if (bitCount > 0) {
                    textContent += "\n";
                }
                textContent += std::to_string(lineNumber) + " ";
                lineNumber++;
            }
            
            // Agregar el bit
            textContent += std::to_string(bit);
            
            // Agregar espacio entre bits
            if ((bitCount + 1) % bitsPerLine != 0 && bitCount + 1 < sb.s_inodes_count) {
                textContent += " ";
            }
            
            bitCount++;
        }
    }
    
    delete[] bitmapData;
    
    // Agregar salto de línea al final si es necesario
    if (!textContent.empty() && textContent.back() != '\n') {
        textContent += "\n";
    }
    
    return saveTextReport(repPath, textContent);
}

std::string ReportGenerator::generateBmBloc(const std::string& diskPath, const std::string& repPath,
                                           const Superblock& sb) {
    std::string textContent = "";
    
    // Calcular cantidad de bytes en el bitmap de bloques
    int bitmapBytes = (sb.s_blocks_count + 7) / 8;
    
    // Leer el bitmap desde el disco
    char* bitmapData = new char[bitmapBytes];
    if (!DiskManager::readFromDisk(diskPath, sb.s_bm_block_start, bitmapData, bitmapBytes)) {
        delete[] bitmapData;
        return "Error: No se pudo leer el bitmap de bloques.";
    }
    
    // Generar contenido del reporte con 20 bits por línea
    int lineNumber = 1;
    int bitsPerLine = 20;
    int bitCount = 0;
    
    for (int byteIdx = 0; byteIdx < bitmapBytes; byteIdx++) {
        unsigned char currentByte = (unsigned char)bitmapData[byteIdx];
        
        // Procesar cada bit del byte
        for (int bitIdx = 0; bitIdx < 8 && bitCount < sb.s_blocks_count; bitIdx++) {
            // Extraer el bit (LSB primero)
            int bit = (currentByte >> bitIdx) & 1;
            
            // Si es el primer bit de la línea, agregar número de línea
            if (bitCount % bitsPerLine == 0) {
                if (bitCount > 0) {
                    textContent += "\n";
                }
                textContent += std::to_string(lineNumber) + " ";
                lineNumber++;
            }
            
            // Agregar el bit
            textContent += std::to_string(bit);
            
            // Agregar espacio entre bits
            if ((bitCount + 1) % bitsPerLine != 0 && bitCount + 1 < sb.s_blocks_count) {
                textContent += " ";
            }
            
            bitCount++;
        }
    }
    
    delete[] bitmapData;
    
    // Agregar salto de línea al final si es necesario
    if (!textContent.empty() && textContent.back() != '\n') {
        textContent += "\n";
    }
    
    return saveTextReport(repPath, textContent);
}

std::string ReportGenerator::generateTree(const std::string& diskPath, int partStart,
                                         const std::string& repPath, const Superblock& sb) {
    std::string graphvizContent = "digraph FileSystemTree {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  edge [fontsize=10];\n";
    
    // Estructura para rastrear nodos y evitar ciclos
    std::set<int> visitedInodes;
    std::string edgesContent = "";
    
    // Función lambda para procesar inodos recursivamente
    std::function<void(int, int, const std::string&)> processInode = 
    [&](int inodeNum, int parentNum, const std::string& parentName) {
        if (visitedInodes.count(inodeNum) > 0) {
            return;  // Evitar ciclos
        }
        visitedInodes.insert(inodeNum);
        
        Inodo ino;
        if (!DiskManager::readInodo(diskPath, partStart, inodeNum, ino)) {
            return;
        }
        
        if (ino.i_type < 0) {
            return;  // Inodo no utilizado
        }
        
        std::string nodeId = "inode_" + std::to_string(inodeNum);
        
        // Convertir timestamp
        std::string atimeStr = std::string(ctime(&ino.i_atime));
        if (!atimeStr.empty() && atimeStr.back() == '\n') {
            atimeStr.pop_back();
        }
        
        // Crear nodo con información del inodo
        graphvizContent += "  " + nodeId + " [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
        
        if (ino.i_type == 0) {
            // Es un directorio
            graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Dir: Inode " + std::to_string(inodeNum) + "</FONT></B></TD></TR>\n";
        } else {
            // Es un archivo
            graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">File: Inode " + std::to_string(inodeNum) + "</FONT></B></TD></TR>\n";
        }
        
        graphvizContent += "      <TR><TD>i_uid</TD><TD>" + std::to_string(ino.i_uid) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>i_size</TD><TD>" + std::to_string(ino.i_s) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>i_atime</TD><TD><FONT SIZE=\"8\">" + atimeStr + "</FONT></TD></TR>\n";
        
        // Mostrar bloques
        for (int i = 0; i < 15 && ino.i_block[i] != -1; i++) {
            graphvizContent += "      <TR><TD>i_block[" + std::to_string(i) + "]</TD><TD>" + std::to_string(ino.i_block[i]) + "</TD></TR>\n";
        }
        
        graphvizContent += "      <TR><TD>i_perm</TD><TD>" + std::string(ino.i_perm) + "</TD></TR>\n";
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        
        // Conectar con padre si existe
        if (parentNum >= 0) {
            std::string parentNodeId = "inode_" + std::to_string(parentNum);
            edgesContent += "  " + parentNodeId + " -> " + nodeId + ";\n";
        }
        
        // Si es directorio, procesar entradas
        if (ino.i_type == 0) {
            for (int blockIdx = 0; blockIdx < 15 && ino.i_block[blockIdx] != -1; blockIdx++) {
                BlockFolder dirBlock;
                if (DiskManager::readBlock(diskPath, partStart, ino.i_block[blockIdx], 
                                          (char*)&dirBlock, sizeof(BlockFolder))) {
                    
                    for (int entryIdx = 0; entryIdx < 4; entryIdx++) {
                        if (dirBlock.b_content[entryIdx].b_inodo > 0) {
                            processInode(dirBlock.b_content[entryIdx].b_inodo, inodeNum, 
                                        std::string(dirBlock.b_content[entryIdx].b_name));
                        }
                    }
                }
            }
        }
    };
    
    // Iniciar desde el inodo raíz
    processInode(0, -1, "/");
    
    // Agregar aristas
    graphvizContent += edgesContent;
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

// Función helper para navegar rutas y obtener inodo
int ReportGenerator::getInodeFromPath(const std::string& diskPath, int partStart, 
                                     const std::string& path, Inodo& resultIno, 
                                     const Superblock& sb) {
    // Parsear la ruta en partes
    std::vector<std::string> parts;
    std::string current = "";
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current = "";
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    // Empezar desde raíz (inodo 0)
    int currentInode = 0;
    Inodo currentInoData;
    
    // Navegar la ruta
    for (size_t i = 0; i < parts.size(); i++) {
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, currentInoData)) {
            return -1;
        }
        
        // Debe ser un directorio para continuar
        if (currentInoData.i_type != 0) {
            return -1;  // Ruta inválida, no es directorio
        }
        
        // Buscar la parte actual en el directorio (buscar en todos los bloques)
        int nextInode = -1;
        for (int blockIdx = 0; blockIdx < 15 && currentInoData.i_block[blockIdx] != -1; blockIdx++) {
            BlockFolder dirBlock;
            if (!DiskManager::readBlock(diskPath, partStart, currentInoData.i_block[blockIdx], 
                                    (char*)&dirBlock, sizeof(BlockFolder))) {
                continue;
            }
            
            for (int j = 0; j < 4; j++) {
                if (dirBlock.b_content[j].b_inodo != 0 && 
                    std::string(dirBlock.b_content[j].b_name) == parts[i]) {
                    nextInode = dirBlock.b_content[j].b_inodo;
                    break;
                }
            }
            if (nextInode >= 0) break;
        }
        
        if (nextInode < 0) {
            return -1;  // Parte no encontrada
        }
        
        currentInode = nextInode;
        
        if (i == parts.size() - 1) {
            // Última parte - es el target (ya verificada que existe)
            resultIno = currentInoData;
            return currentInode;
        }
    }
    
    return currentInode;
}

std::string ReportGenerator::generateFile(const std::string& diskPath, int partStart,
                                         const std::string& repPath, const Superblock& sb,
                                         const std::map<std::string, std::string>& params) {
    // Obtener -ruta del mapa de parámetros
    auto it = params.find("ruta");
    if (it == params.end()) {
        return "Error: Parámetro -ruta requerido para reporte file";
    }
    std::string filePath = it->second;
    
    // Extraer directorio y nombre del archivo
    std::string dirPath = "/";
    std::string fileName = filePath;
    
    size_t lastSlash = filePath.find_last_of("/");
    if (lastSlash != std::string::npos) {
        dirPath = filePath.substr(0, lastSlash);
        if (dirPath.empty()) dirPath = "/";
        fileName = filePath.substr(lastSlash + 1);
    }
    
    // Obtener inodo del directorio padre
    Inodo dirInoData;
    int dirInoNum = 0;
    
    if (dirPath != "/") {
        dirInoNum = getInodeFromPath(diskPath, partStart, dirPath, dirInoData, sb);
        if (dirInoNum < 0) {
            return "Error: El directorio '" + dirPath + "' no existe.";
        }
        // Leer el inodo correcto usando el número retornado
        if (!DiskManager::readInodo(diskPath, partStart, dirInoNum, dirInoData)) {
            return "Error: No se pudo leer el inodo del directorio.";
        }
    } else {
        // Leer inodo raíz
        if (!DiskManager::readInodo(diskPath, partStart, 0, dirInoData)) {
            return "Error: No se pudo leer el inodo raíz.";
        }
    }
    
    // Buscar archivo en los bloques del directorio
    int fileInode = -1;
    for (int blockIdx = 0; blockIdx < 15 && dirInoData.i_block[blockIdx] != -1; blockIdx++) {
        BlockFolder dirBlock;
        if (!DiskManager::readBlock(diskPath, partStart, dirInoData.i_block[blockIdx], 
                                (char*)&dirBlock, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (std::string(dirBlock.b_content[j].b_name) == fileName && dirBlock.b_content[j].b_inodo != 0) {
                fileInode = dirBlock.b_content[j].b_inodo;
                break;
            }
        }
        
        if (fileInode >= 0) break;
    }
    
    if (fileInode < 0) {
        return "Error: El archivo '" + fileName + "' no existe.";
    }
    
    // Leer inodo del archivo
    Inodo fileIno;
    if (!DiskManager::readInodo(diskPath, partStart, fileInode, fileIno)) {
        return "Error: No se pudo leer el inodo del archivo.";
    }
    
    // Generar reporte Graphviz
    std::string graphvizContent = "digraph FileInfo {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  FileInfo [label=<\n";
    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
    // Escapar caracteres especiales en el nombre del archivo
    std::string escapedFileName = fileName;
    size_t pos = 0;
    while ((pos = escapedFileName.find("&", pos)) != std::string::npos) {
        escapedFileName.replace(pos, 1, "&amp;");
        pos += 5;
    }
    pos = 0;
    while ((pos = escapedFileName.find("<", pos)) != std::string::npos) {
        escapedFileName.replace(pos, 1, "&lt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = escapedFileName.find(">", pos)) != std::string::npos) {
        escapedFileName.replace(pos, 1, "&gt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = escapedFileName.find("\"", pos)) != std::string::npos) {
        escapedFileName.replace(pos, 1, "&quot;");
        pos += 6;
    }
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">INFORMACIÓN DEL ARCHIVO: " + escapedFileName + "</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>Inodo</TD><TD>" + std::to_string(fileInode) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>UID</TD><TD>" + std::to_string(fileIno.i_uid) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>GID</TD><TD>" + std::to_string(fileIno.i_gid) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>Tipo</TD><TD>" + std::string(fileIno.i_type == 1 ? "Archivo" : "Directorio") + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>Permisos</TD><TD>" + escapeHTMLSpecialChars(std::string(fileIno.i_perm)) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>Tamaño</TD><TD>" + std::to_string(fileIno.i_s) + " bytes</TD></TR>\n";
    
    char atimeStr[64];
    struct tm* timeinfo = localtime(&fileIno.i_atime);
    strftime(atimeStr, sizeof(atimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    graphvizContent += "      <TR><TD>Acceso (atime)</TD><TD>" + escapeHTMLSpecialChars(std::string(atimeStr)) + "</TD></TR>\n";
    
    char ctimeStr[64];
    timeinfo = localtime(&fileIno.i_ctime);
    strftime(ctimeStr, sizeof(ctimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    graphvizContent += "      <TR><TD>Cambio (ctime)</TD><TD>" + escapeHTMLSpecialChars(std::string(ctimeStr)) + "</TD></TR>\n";
    
    char mtimeStr[64];
    timeinfo = localtime(&fileIno.i_mtime);
    strftime(mtimeStr, sizeof(mtimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    graphvizContent += "      <TR><TD>Modificación (mtime)</TD><TD>" + escapeHTMLSpecialChars(std::string(mtimeStr)) + "</TD></TR>\n";
    
    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">BLOQUES ASIGNADOS</FONT></B></TD></TR>\n";
    bool hasBlocks = false;
    for (int i = 0; i < 15; i++) {
        if (fileIno.i_block[i] != -1) {
            graphvizContent += "      <TR><TD>Bloque " + std::to_string(i) + "</TD><TD>" + std::to_string(fileIno.i_block[i]) + "</TD></TR>\n";
            hasBlocks = true;
        }
    }
    if (!hasBlocks) {
        graphvizContent += "      <TR><TD COLSPAN=\"2\">Sin bloques asignados</TD></TR>\n";
    }
    
    graphvizContent += "    </TABLE>\n";
    graphvizContent += "  >];\n";
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateLS(const std::string& diskPath, int partStart,
                                       const std::string& repPath, const Superblock& sb,
                                       const std::map<std::string, std::string>& params) {
    // Obtener -ruta del mapa de parámetros
    auto it = params.find("ruta");
    if (it == params.end()) {
        return "Error: Parámetro -ruta requerido para reporte ls";
    }
    std::string dirPath = it->second;
    
    // Obtener inodo del directorio
    Inodo dirInoData;
    int dirInoNum = 0;
    
    if (dirPath != "/") {
        dirInoNum = getInodeFromPath(diskPath, partStart, dirPath, dirInoData, sb);
        if (dirInoNum < 0) {
            return "Error: El directorio '" + dirPath + "' no existe.";
        }
        // Leer el inodo correcto usando el número retornado
        if (!DiskManager::readInodo(diskPath, partStart, dirInoNum, dirInoData)) {
            return "Error: No se pudo leer el inodo del directorio.";
        }
    } else {
        // Leer inodo raíz
        if (!DiskManager::readInodo(diskPath, partStart, 0, dirInoData)) {
            return "Error: No se pudo leer el inodo raíz.";
        }
        dirInoNum = 0;
    }
    
    // Verificar que es un directorio
    if (dirInoData.i_type != 0) {
        return "Error: '" + dirPath + "' no es un directorio.";
    }
    
    // Generar reporte Graphviz
    std::string graphvizContent = "digraph LSInfo {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  LSInfo [label=<\n";
    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
    // Escapar caracteres especiales en la ruta del directorio
    std::string escapedDirPath = dirPath;
    size_t pos = 0;
    while ((pos = escapedDirPath.find("&", pos)) != std::string::npos) {
        escapedDirPath.replace(pos, 1, "&amp;");
        pos += 5;
    }
    pos = 0;
    while ((pos = escapedDirPath.find("<", pos)) != std::string::npos) {
        escapedDirPath.replace(pos, 1, "&lt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = escapedDirPath.find(">", pos)) != std::string::npos) {
        escapedDirPath.replace(pos, 1, "&gt;");
        pos += 4;
    }
    pos = 0;
    while ((pos = escapedDirPath.find("\"", pos)) != std::string::npos) {
        escapedDirPath.replace(pos, 1, "&quot;");
        pos += 6;
    }
    graphvizContent += "      <TR><TD COLSPAN=\"5\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">CONTENIDO DEL DIRECTORIO: " + escapedDirPath + "</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD><B>Nombre</B></TD><TD><B>Inodo</B></TD><TD><B>Tipo</B></TD><TD><B>Tamaño</B></TD><TD><B>Permisos</B></TD></TR>\n";
    
    int entryCount = 0;
    
    // Iterar por todos los bloques del directorio
    for (int blockIdx = 0; blockIdx < 15 && dirInoData.i_block[blockIdx] != -1; blockIdx++) {
        BlockFolder dirBlock;
        if (!DiskManager::readBlock(diskPath, partStart, dirInoData.i_block[blockIdx], 
                                (char*)&dirBlock, sizeof(BlockFolder))) {
            continue;
        }
        
        // Procesar cada entrada del bloque
        for (int j = 0; j < 4; j++) {
            if (dirBlock.b_content[j].b_inodo != 0) {
                int entryInode = dirBlock.b_content[j].b_inodo;
                std::string entryName = dirBlock.b_content[j].b_name;
                
                // Leer inodo de la entrada
                Inodo entryInoData;
                if (!DiskManager::readInodo(diskPath, partStart, entryInode, entryInoData)) {
                    continue;
                }
                
                std::string type = (entryInoData.i_type == 0) ? "Directorio" : "Archivo";
                
                graphvizContent += "      <TR>";
                graphvizContent += "<TD>" + escapeHTMLSpecialChars(entryName) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(entryInode) + "</TD>";
                graphvizContent += "<TD>" + type + "</TD>";
                graphvizContent += "<TD>" + std::to_string(entryInoData.i_s) + "</TD>";
                graphvizContent += "<TD>" + escapeHTMLSpecialChars(std::string(entryInoData.i_perm)) + "</TD>";
                graphvizContent += "</TR>\n";
                
                entryCount++;
            }
        }
    }
    
    if (entryCount == 0) {
        graphvizContent += "      <TR><TD COLSPAN=\"5\">Directorio vacío</TD></TR>\n";
    }
    
    graphvizContent += "    </TABLE>\n";
    graphvizContent += "  >];\n";
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateDisk(const std::string& diskPath, const std::string& repPath,
                                         const MBR& mbr) {
    int diskSize = (int)DiskManager::getFileSize(diskPath);
    
    // Recolectar información de particiones
    struct PartInfo {
        std::string name;
        int start;
        int size;
        char type;
        char status;
        int index;
    };
    std::vector<PartInfo> partitions;
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            partitions.push_back({
                std::string(mbr.mbr_partitions[i].part_name),
                mbr.mbr_partitions[i].part_start,
                mbr.mbr_partitions[i].part_s,
                mbr.mbr_partitions[i].part_type,
                mbr.mbr_partitions[i].part_status,
                i
            });
        }
    }
    
    // Calcular espacios libres
    std::vector<PartInfo> freeSpaces;
    int currentPos = sizeof(MBR);
    for (const auto& part : partitions) {
        if (part.start > currentPos) {
            freeSpaces.push_back({
                "Libre",
                currentPos,
                part.start - currentPos,
                'F',
                '0',
                -1
            });
        }
        currentPos = part.start + part.size;
    }
    
    if (currentPos < diskSize) {
        freeSpaces.push_back({
            "Libre",
            currentPos,
            diskSize - currentPos,
            'F',
            '0',
            -1
        });
    }
    
    // Generar reporte
    std::string graphvizContent = "digraph Disk {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    graphvizContent += "  DiskReport [label=<\n";
    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
    
    // Encabezado
    graphvizContent += "      <TR><TD COLSPAN=\"7\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">REPORTE DE DISCO</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_tamano</TD><TD COLSPAN=\"6\">" + std::to_string(diskSize) + " bytes</TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_fecha_creacion</TD><TD COLSPAN=\"6\">" + std::string(__DATE__) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD>mbr_dsk_signature</TD><TD COLSPAN=\"6\">" + std::to_string(mbr.mbr_dsk_signature) + "</TD></TR>\n";
    graphvizContent += "      <TR><TD COLSPAN=\"7\"></TD></TR>\n";
    
    // Tabla de particiones con porcentajes
    graphvizContent += "      <TR><TD COLSPAN=\"7\" BGCOLOR=\"#A020F0\"><B><FONT COLOR=\"white\">Particiones del Disco</FONT></B></TD></TR>\n";
    graphvizContent += "      <TR><TD><B>Nombre</B></TD><TD><B>Tipo</B></TD><TD><B>Inicio</B></TD><TD><B>Tamaño</B></TD><TD><B>Bytes</B></TD><TD><B>Porcentaje</B></TD><TD><B>Barra</B></TD></TR>\n";
    
    // Mostrar particiones
    for (const auto& part : partitions) {
        double percentage = (diskSize > 0) ? (part.size * 100.0 / diskSize) : 0;
        int barLength = (int)(percentage / 2);
        std::string bar = "";
        for (int i = 0; i < barLength; i++) bar += "█";
        
        std::string typeStr = "";
        std::string bgColor = "#E8E8E8";
        if (part.type == 'P') { typeStr = "Primaria"; bgColor = "#FFE4B5"; }
        else if (part.type == 'E') { typeStr = "Extendida"; bgColor = "#FFB6C1"; }
        else if (part.type == 'L') { typeStr = "Lógica"; bgColor = "#ADD8E6"; }
        
        graphvizContent += "      <TR><TD>" + part.name + "</TD>";
        graphvizContent += "<TD>" + typeStr + "</TD>";
        graphvizContent += "<TD>" + std::to_string(part.start) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(part.size) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(part.size) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(percentage).substr(0, 5) + "%</TD>";
        graphvizContent += "<TD BGCOLOR=\"" + bgColor + "\">" + bar + "</TD></TR>\n";
    }
    
    // Mostrar espacios libres
    for (const auto& freeSpace : freeSpaces) {
        double percentage = (diskSize > 0) ? (freeSpace.size * 100.0 / diskSize) : 0;
        int barLength = (int)(percentage / 2);
        std::string bar = "";
        for (int i = 0; i < barLength; i++) bar += "░";
        
        graphvizContent += "      <TR><TD>" + freeSpace.name + "</TD>";
        graphvizContent += "<TD>-</TD>";
        graphvizContent += "<TD>" + std::to_string(freeSpace.start) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(freeSpace.size) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(freeSpace.size) + "</TD>";
        graphvizContent += "<TD>" + std::to_string(percentage).substr(0, 5) + "%</TD>";
        graphvizContent += "<TD BGCOLOR=\"#D3D3D3\">" + bar + "</TD></TR>\n";
    }
    
    // Fila de totales
    double totalUsed = 0;
    for (const auto& part : partitions) totalUsed += part.size;
    double totalPercentage = (diskSize > 0) ? (totalUsed * 100.0 / diskSize) : 0;
    
    graphvizContent += "      <TR><TD COLSPAN=\"7\"></TD></TR>\n";
    graphvizContent += "      <TR>\n";
    graphvizContent += "        <TD><B>TOTAL USADO</B></TD>\n";
    graphvizContent += "        <TD>-</TD>\n";
    graphvizContent += "        <TD>-</TD>\n";
    graphvizContent += "        <TD>" + std::to_string((int)totalUsed) + "</TD>\n";
    graphvizContent += "        <TD>" + std::to_string(diskSize) + "</TD>\n";
    graphvizContent += "        <TD><B>" + std::to_string(totalPercentage).substr(0, 5) + "%</B></TD>\n";
    graphvizContent += "        <TD>-</TD>\n";
    graphvizContent += "      </TR>\n";
    graphvizContent += "    </TABLE>\n";
    graphvizContent += "  >];\n";
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

std::string ReportGenerator::generateEBR(const std::string& diskPath, const std::string& repPath,
                                        const Partition* part) {
    // Verificar que la partición es extendida
    if (part->part_type != 'E') {
        return "Error: La partición no es extendida. No hay EBRs para este tipo de partición.";
    }
    
    std::string graphvizContent = "digraph EBR {\n";
    graphvizContent += "  rankdir=TB;\n";
    graphvizContent += "  node [shape=plaintext];\n";
    
    // Leer EBRs de la cadena de particiones lógicas
    int ebr_offset = part->part_start;
    bool first = true;
    
    while (ebr_offset > 0) {
        EBR ebr;
        if (!DiskManager::readFromDisk(diskPath, ebr_offset, (char*)&ebr, sizeof(EBR))) {
            break;
        }
        
        if (ebr.part_s == 0 && ebr.part_start < 0) {
            break;  // EBR vacío
        }
        
        // Crear tabla para esta partición
        std::string nodeId = "ebr_" + std::to_string(ebr_offset);
        graphvizContent += "  " + nodeId + " [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#E8E8E8\">\n";
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">Partición</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>part_mount</TD><TD>" + std::string(1, ebr.part_mount) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>part_type</TD><TD>L</TD></TR>\n";
        graphvizContent += "      <TR><TD>part_fit</TD><TD>" + std::string(1, ebr.part_fit) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>part_start</TD><TD>" + std::to_string(ebr.part_start) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>part_size</TD><TD>" + std::to_string(ebr.part_s) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>part_name</TD><TD>" + std::string(ebr.part_name) + "</TD></TR>\n";
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        
        // Apuntar al siguiente EBR si existe
        if (ebr.part_next > 0) {
            std::string nextNodeId = "ebr_" + std::to_string(ebr.part_next);
            graphvizContent += "  " + nodeId + " -> " + nextNodeId + " [label=\"next\"];\n";
        }
        
        ebr_offset = ebr.part_next;
    }
    
    graphvizContent += "}\n";
    
    return saveGraphvizReport(repPath, graphvizContent);
}

// ============ MÉTODOS HELPER ============

std::string ReportGenerator::saveGraphvizReport(const std::string& repPath, const std::string& content) {
    // Remover comillas si existen
    std::string cleanPath = repPath;
    if (!cleanPath.empty() && cleanPath.back() == '"') {
        cleanPath.pop_back();
    }
    if (!cleanPath.empty() && cleanPath[0] == '"') {
        cleanPath = cleanPath.substr(1);
    }
    
    // Crear archivo .dot temporal
    std::string dotFilePath = cleanPath;
    std::string outputFilePath = cleanPath;
    
    if (cleanPath.find(".dot") == std::string::npos && 
        cleanPath.find(".jpg") == std::string::npos &&
        cleanPath.find(".png") == std::string::npos) {
        dotFilePath += ".dot";
        outputFilePath += ".jpg";
    } else if (cleanPath.find(".jpg") != std::string::npos ||
               cleanPath.find(".jpeg") != std::string::npos) {
        dotFilePath = cleanPath.substr(0, cleanPath.find_last_of(".")) + ".dot";
        outputFilePath = cleanPath;
    } else if (cleanPath.find(".png") != std::string::npos) {
        dotFilePath = cleanPath.substr(0, cleanPath.find_last_of(".")) + ".dot";
        outputFilePath = cleanPath;
    } else {
        dotFilePath = cleanPath + ".dot";
        outputFilePath = cleanPath + ".jpg";
    }
    
    // Guardar archivo .dot
    std::ofstream dotFile(dotFilePath);
    if (!dotFile.is_open()) {
        return "Error: No se pudo crear el archivo de reporte: " + dotFilePath;
    }
    
    dotFile << content;
    dotFile.close();
    
    // Convertir a imagen con graphviz
    std::string format = "jpg";
    if (outputFilePath.find(".png") != std::string::npos) {
        format = "png";
    }
    
    std::string errorFile = dotFilePath + ".err";
    std::string command = "dot -T" + format + " \"" + dotFilePath + "\" -o \"" + outputFilePath + "\" 2>\"" + errorFile + "\"";
    int result = system(command.c_str());
    
    if (result != 0) {
        // Leer el archivo de error para obtener más información
        std::ifstream errStream(errorFile);
        std::string errorMsg;
        if (errStream.is_open()) {
            std::string line;
            while (std::getline(errStream, line)) {
                errorMsg += line + "\n";
            }
            errStream.close();
        }
        system(("rm -f \"" + errorFile + "\"").c_str());
        return "Error: No se pudo generar la imagen. Se guardó el reporte en formato .dot: " + dotFilePath + (errorMsg.empty() ? "" : " (" + errorMsg + ")");
    }
    
    // Limpiar archivos temporales
    system(("rm -f \"" + dotFilePath + "\"").c_str());
    system(("rm -f \"" + errorFile + "\"").c_str());
    
    // Extract report name
    std::string repName = outputFilePath;
    size_t lastSlash = repName.find_last_of("/");
    if (lastSlash != std::string::npos) {
        repName = repName.substr(lastSlash + 1);
    }
    
    return "Reporte " + repName + " generado: " + outputFilePath;
}

std::string ReportGenerator::saveTextReport(const std::string& repPath, const std::string& content) {
    // Remover comillas si existen
    std::string cleanPath = repPath;
    if (!cleanPath.empty() && cleanPath.back() == '"') {
        cleanPath.pop_back();
    }
    if (!cleanPath.empty() && cleanPath[0] == '"') {
        cleanPath = cleanPath.substr(1);
    }
    
    std::ofstream textFile(cleanPath);
    if (!textFile.is_open()) {
        return "Error: No se pudo crear el archivo de reporte: " + cleanPath;
    }
    
    textFile << content;
    textFile.close();
    
    return "Reporte generado: " + cleanPath;
}

std::string ReportGenerator::permissionsToRwx(const std::string& permissions) {
    if (permissions.empty()) {
        return "---------";
    }
    
    std::string result = "";
    for (char c : permissions) {
        int digit = c - '0';
        result += ((digit & 4) ? 'r' : '-');
        result += ((digit & 2) ? 'w' : '-');
        result += ((digit & 1) ? 'x' : '-');
    }
    
    return result;
}

std::string ReportGenerator::formatDate(time_t timestamp) {
    std::string dateStr = std::string(ctime(&timestamp));
    if (!dateStr.empty() && dateStr.back() == '\n') {
        dateStr.pop_back();
    }
    return dateStr;
}
