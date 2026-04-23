#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <map>
#include <vector>
#include <set>
#include "structures.h"
#include "disk_manager.h"

class CommandHandler {
private:
    std::map<std::string, std::string> params;
    std::string currentUser;
    std::string currentPartitionId;
    bool isLoggedIn;
    int currentUserUid;
    int currentUserGid;
    
    // Variables estáticas para persistencia entre peticiones HTTP
    static std::map<std::string, std::string> mountedPartitions;  // ID -> ruta del disco
    static std::map<std::string, int> diskMountStates;  // ruta del disco -> número de montaje
    static std::map<std::string, char> diskLetterStates;  // ruta del disco -> letra actual
    static std::map<std::string, std::pair<std::string, int>> partitionInfo;  // ID -> (diskPath, partitionIndex)
    static std::map<std::string, std::string> partitionNames;  // ID -> nombre de la partición
    static std::map<std::string, std::string> groups;  // groupName -> groupId
    static std::map<std::string, std::string> users;   // userName -> password:groupName
    static std::set<std::string> mountedDiskPartitions;  // diskPath:partName -> rastrear mounts duplicados

public:
    CommandHandler();
    
    // Procesa un comando completo
    std::string processCommand(const std::string& line);
    
    // Obtiene el usuario actual
    std::string getCurrentUser() const;
    
    // Obtiene el ID de partición actual
    std::string getCurrentPartitionId() const;

private:
    // Comandos de disco
    std::string cmdMkdisk(const std::map<std::string, std::string>& params);
    std::string cmdRmdisk(const std::map<std::string, std::string>& params);
    std::string cmdFdisk(const std::map<std::string, std::string>& params);
    std::string cmdFdiskDelete(const std::string& path, const std::string& name, const std::string& deleteType);
    std::string cmdFdiskAdd(const std::string& path, const std::string& name, const std::string& addStr,
                            const std::map<std::string, std::string>& params);
    std::string cmdMount(const std::map<std::string, std::string>& params);
    std::string cmdUnmount(const std::map<std::string, std::string>& params);
    std::string cmdMounted(const std::map<std::string, std::string>& params);
    
    // Comandos del sistema de archivos
    std::string cmdMkfs(const std::map<std::string, std::string>& params);
    std::string cmdLogin(const std::map<std::string, std::string>& params);
    std::string cmdLogout(const std::map<std::string, std::string>& params);
    std::string cmdCat(const std::map<std::string, std::string>& params);
    
    // Comandos de usuarios y grupos
    std::string cmdMkgrp(const std::map<std::string, std::string>& params);
    std::string cmdRmgrp(const std::map<std::string, std::string>& params);
    std::string cmdMkusr(const std::map<std::string, std::string>& params);
    std::string cmdRmusr(const std::map<std::string, std::string>& params);
    std::string cmdChgrp(const std::map<std::string, std::string>& params);
    std::string cmdChown(const std::map<std::string, std::string>& params);
    std::string cmdChmod(const std::map<std::string, std::string>& params);
    std::string cmdLoss(const std::map<std::string, std::string>& params);
    std::string cmdJournaling(const std::map<std::string, std::string>& params);
    
    // Comandos de archivos y carpetas
    std::string cmdMkdir(const std::map<std::string, std::string>& params);
    std::string cmdMkfile(const std::map<std::string, std::string>& params);
    std::string cmdRemove(const std::map<std::string, std::string>& params);
    std::string cmdRename(const std::map<std::string, std::string>& params);
    std::string cmdCopy(const std::map<std::string, std::string>& params);
    std::string cmdMove(const std::map<std::string, std::string>& params);
    std::string cmdFind(const std::map<std::string, std::string>& params);
    
    // Helper para copia recursiva
    bool copyRecursive(const std::string& diskPath, int partStart, int srcInodeNum, int destInodeNum,
                       const std::string& srcName, int uid, int gid, Superblock& sb);
    
    // Reportes
    std::string cmdRep(const std::map<std::string, std::string>& params);
    
    // Funciones helper para filesystem
    struct PathNode {
        std::string fileName;
        int inodeNum;
        char type;  
    };
    
    // Obtiene la partición por ID (retorna nullptr si no existe)
    Partition* findPartitionById(const std::string& id, const MBR& mbr);
    
    // Obtiene un inodo por número
    bool getInodeFromNum(const std::string& diskPath, int partStart, int inodeNum, Inodo& ino);
    
    // Busca un archivo/carpeta por nombre en un directorio
    int findInodeInDirectory(const std::string& diskPath, int partStart, int parentInode, 
                            const std::string& name, Inodo& parentIno);
    
    // Obtiene el inodo de una ruta completa 
    int getInodeFromPath(const std::string& diskPath, int partStart, const std::string& path,
                        Inodo& resultIno, char& type);
    
    // Crea recursivamente las carpetas padres de una ruta
    bool createParentDirs(const std::string& diskPath, int partStart, int superblockStart,
                        const std::string& path);
    
    // Valida parámetros obligatorios
    std::string validateMandatoryParams(const std::map<std::string, std::string>& params,
                                    const std::vector<std::string>& mandatory);
    
    // Valida que solo se usen parámetros permitidos
    std::string validateAllowedParams(const std::map<std::string, std::string>& params,
                                    const std::vector<std::string>& allowed);
    
    // Helper para validar permisos UGO
    bool hasWritePermission(const Inodo& ino, int uid, int gid);
    bool hasReadPermission(const Inodo& ino, int uid, int gid);
    
    // Helper para eliminar recursivamente
    bool removeRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                         int uid, int gid, Superblock& sb);
    
    // Helper para búsqueda con wildcards
    bool matchPattern(const std::string& name, const std::string& pattern);
    void findRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                      const std::string& pattern, int uid, int gid,
                      std::vector<std::pair<std::string, int>>& results);
    
    // Helper para chown recursivo
    bool chownRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                       int newUid, int currentUid, bool isRoot);
    
    // Helper para chmod recursivo
    bool chmodRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                       const std::string& perms, int currentUid, bool recursive);
};

#endif // COMMAND_HANDLER_H
