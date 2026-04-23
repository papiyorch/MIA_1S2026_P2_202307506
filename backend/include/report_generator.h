#pragma once

#include <string>
#include <map>
#include "disk_manager.h"

class ReportGenerator {
public:
    // Método principal para generar reportes delegando a métodos específicos
    std::string generateReport(const std::string& repName, const std::string& repPath,
                            const std::string& partitionId, const std::string& diskPath,
                            const MBR& mbr, const Partition* part,
                            const std::map<std::string, std::string>& params = {});
    
private:
    // Métodos específicos para cada tipo de reporte
    std::string generateMBR(const std::string& repPath, const MBR& mbr);
    std::string generateSB(const std::string& repPath, const Superblock& sb);
    std::string generateInode(const std::string& diskPath, int partStart,
                            const std::string& repPath, const Superblock& sb);
    std::string generateBlock(const std::string& diskPath, int partStart,
                            const std::string& repPath, const Superblock& sb);
    std::string generateBmInode(const std::string& diskPath, const std::string& repPath,
                            const Superblock& sb);
    std::string generateBmBloc(const std::string& diskPath, const std::string& repPath,
                            const Superblock& sb);
    std::string generateTree(const std::string& diskPath, int partStart,
                            const std::string& repPath, const Superblock& sb);
    std::string generateFile(const std::string& diskPath, int partStart,
                            const std::string& repPath, const Superblock& sb,
                            const std::map<std::string, std::string>& params = {});
    std::string generateLS(const std::string& diskPath, int partStart,
                        const std::string& repPath, const Superblock& sb,
                        const std::map<std::string, std::string>& params = {});
    std::string generateDisk(const std::string& diskPath, const std::string& repPath,
                            const MBR& mbr);
    std::string generateEBR(const std::string& diskPath, const std::string& repPath,
                        const Partition* part);
    
    // Funciones auxiliares
    std::string saveGraphvizReport(const std::string& repPath, const std::string& content);
    std::string saveTextReport(const std::string& repPath, const std::string& content);
    std::string permissionsToRwx(const std::string& permissions);
    std::string formatDate(time_t timestamp);
    
    // Función helper para navegar rutas y obtener inodo
    int getInodeFromPath(const std::string& diskPath, int partStart, const std::string& path, 
                        Inodo& resultIno, const Superblock& sb);
};
