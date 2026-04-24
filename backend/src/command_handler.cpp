#include "command_handler.h"
#include "command_parser.h"
#include "report_generator.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <functional>
#include <set>

// Inicializar variables estáticas
std::map<std::string, std::string> CommandHandler::mountedPartitions;
std::map<std::string, int> CommandHandler::diskMountStates;
std::map<std::string, char> CommandHandler::diskLetterStates;
std::map<std::string, std::pair<std::string, int>> CommandHandler::partitionInfo;
std::map<std::string, std::string> CommandHandler::partitionNames;
std::map<std::string, std::string> CommandHandler::groups;
std::map<std::string, std::string> CommandHandler::users;
std::set<std::string> CommandHandler::mountedDiskPartitions;

CommandHandler::CommandHandler() : currentUser(""), currentPartitionId(""), isLoggedIn(false), currentUserUid(0), currentUserGid(0) {}

std::string CommandHandler::processCommand(const std::string& line) {
    std::string commandName = CommandParser::getCommandName(line);
    params = CommandParser::parseCommand(line);
    
    // ====== Comandos que NO requieren sesión activa ======
    // Gestión de discos
    if (commandName == "mkdisk") return cmdMkdisk(params);
    if (commandName == "rmdisk") return cmdRmdisk(params);
    
    // Gestión de particiones
    if (commandName == "fdisk") return cmdFdisk(params);
    
    // Montaje de particiones
    if (commandName == "mount") return cmdMount(params);
    if (commandName == "unmount") return cmdUnmount(params);
    if (commandName == "mounted") return cmdMounted(params);
    
    // Formateo de partición
    if (commandName == "mkfs") return cmdMkfs(params);
    if (commandName == "loss") return cmdLoss(params);
    if (commandName == "journaling") return cmdJournaling(params);

    // Autenticación
    if (commandName == "login") return cmdLogin(params);
    if (commandName == "logout") return cmdLogout(params);
    
    // Reportes de estructura de disco 
    if (commandName == "rep") {
        std::string repName = CommandParser::getParameter(params, "name");
        if (repName == "mbr" || repName == "disk" || repName == "ebr") {
            return cmdRep(params);
        }
    }
    
    // ====== Todos los demás comandos requieren sesión activa ======
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión primero.";
    }
    
    // Gestión de grupos
    if (commandName == "mkgrp") return cmdMkgrp(params);
    if (commandName == "rmgrp") return cmdRmgrp(params);
    
    // Gestión de usuarios
    if (commandName == "mkusr") return cmdMkusr(params);
    if (commandName == "rmusr") return cmdRmusr(params);
    if (commandName == "chgrp") return cmdChgrp(params);
    if (commandName == "chown") return cmdChown(params);
    if (commandName == "chmod") return cmdChmod(params);
    
    // Gestión del filesystem
    if (commandName == "mkdir") return cmdMkdir(params);
    if (commandName == "mkfile") return cmdMkfile(params);
    if (commandName == "remove") return cmdRemove(params);
    if (commandName == "rename") return cmdRename(params);
    if (commandName == "copy") return cmdCopy(params);
    if (commandName == "move") return cmdMove(params);
    if (commandName == "find") return cmdFind(params);
    if (commandName == "cat") return cmdCat(params);
    
    // Reportes del filesystem (requieren sesión)
    if (commandName == "rep") return cmdRep(params);
    
    return "Error: Comando no reconocido: " + commandName;
}

std::string CommandHandler::getCurrentUser() const {
    return currentUser;
}

std::string CommandHandler::getCurrentPartitionId() const {
    return currentPartitionId;
}

// Helper para encontrar partición por ID
Partition* CommandHandler::findPartitionById(const std::string& id, const MBR& mbr) {
    if (partitionInfo.find(id) == partitionInfo.end()) {
        return nullptr;
    }
    int partIndex = partitionInfo[id].second;
    if (partIndex >= 0 && partIndex < 4) {
        return (Partition*)&mbr.mbr_partitions[partIndex];
    }
    return nullptr;
}

std::string CommandHandler::cmdMkdisk(const std::map<std::string, std::string>& params) {
    // Parámetros permitidos: size, path, unit, fit
    std::vector<std::string> allowed = {"size", "path", "unit", "fit"};
    
    // Validar que solo se usen parámetros permitidos
    for (const auto& param : params) {
        bool isAllowed = false;
        for (const auto& allowedParam : allowed) {
            if (param.first == allowedParam) {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed) {
            return "Error: Parámetro no permitido: -" + param.first;
        }
    }
    
    // Parámetros obligatorios: -size, -path
    std::vector<std::string> mandatory = {"size", "path"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    // Obtener parámetros
    std::string sizeStr = CommandParser::getParameter(params, "size");
    std::string path = CommandParser::getParameter(params, "path");
    std::string unitStr = CommandParser::getParameter(params, "unit", "M");
    std::string fitStr = CommandParser::getParameter(params, "fit", "FF");
    
    // Validar que la ruta termine en .mia
    if (path.length() < 4 || path.substr(path.length() - 4) != ".mia") {
        return "Error: El archivo debe tener extensión .mia";
    }
    
    // Validar y convertir tamaño
    int size;
    try {
        size = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Tamaño inválido.";
    }
    
    if (size <= 0) {
        return "Error: Tamaño debe ser mayor que cero.";
    }
    
    // Convertir a bytes - solo K y M
    std::string unitLower = CommandParser::toLower(unitStr);
    if (unitLower == "k") {
        size *= 1024;
    } else if (unitLower == "m") {
        size *= 1024 * 1024;
    } else {
        return "Error: Unidad no válida. Use K o M.";
    }
    
    // Validar fit - debe ser BF, FF o WF
    std::string fitLower = CommandParser::toLower(fitStr);
    char fit = 'F';
    if (fitLower == "bf" || fitLower == "b") {
        fit = 'B';
    } else if (fitLower == "ff" || fitLower == "f") {
        fit = 'F';
    } else if (fitLower == "wf" || fitLower == "w") {
        fit = 'W';
    } else {
        return "Error: Fit no válido. Use BF, FF o WF.";
    }
    
    // Crear disco
    if (DiskManager::createDisk(path, size, fit)) {
        // Leer MBR para obtener signature
        MBR mbr;
        std::string sizeDisplay = sizeStr + CommandParser::toLower(unitStr);
        int signature = 0;
        if (DiskManager::readMBR(path, mbr)) {
            signature = mbr.mbr_dsk_signature;
        }
        return "Disco creado: " + path + " | Tamaño: " + sizeDisplay + " | Signature: " + std::to_string(signature);
    } else {
        return "Error: No se pudo crear el disco.";
    }
}

std::string CommandHandler::cmdRmdisk(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"path"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    
    // Verificar si el archivo existe
    std::ifstream file(path);
    if (!file.good()) {
        return "Error: El archivo no existe: " + path;
    }
    file.close();
    
    if (DiskManager::removeDisk(path)) {
        return "Disco eliminado: " + path;
    } else {
        return "Error: No se pudo eliminar el disco.";
    }
}

std::string CommandHandler::cmdFdisk(const std::map<std::string, std::string>& params) {
    // Parámetros obligatorios: path y name siempre
    std::string path = CommandParser::getParameter(params, "path");
    std::string name = CommandParser::getParameter(params, "name");
    
    if (path.empty() || name.empty()) {
        return "Error: Los parámetros -path y -name son obligatorios.";
    }
    
    // Validar disco existe
    if (!DiskManager::fileExists(path)) {
        return "Error: No se encuentra el disco en la ruta " + path;
    }
    
    // Verificar si es una operación de eliminación
    std::string deleteStr = CommandParser::getParameter(params, "delete");
    if (!deleteStr.empty()) {
        return cmdFdiskDelete(path, name, deleteStr);
    }
    
    // Verificar si es una operación de agregar/quitar espacio
    std::string addStr = CommandParser::getParameter(params, "add");
    if (!addStr.empty()) {
        return cmdFdiskAdd(path, name, addStr, params);
    }
    
    // Si no hay delete ni add, es una creación de partición
    std::vector<std::string> mandatory = {"size"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string sizeStr = CommandParser::getParameter(params, "size");
    std::string unitStr = CommandParser::getParameter(params, "unit", "K");
    std::string typeStr = CommandParser::getParameter(params, "type", "P");
    std::string fitStr = CommandParser::getParameter(params, "fit", "WF");  // Default: WF
    
    // Validar disco existe
    if (!DiskManager::fileExists(path)) {
        return "Error: No se encuentra el disco en la ruta " + path;
    }
    
    // Convertir tamaño
    int size;
    try {
        size = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Tamaño inválido.";
    }
    
    if (size <= 0) {
        return "Error: Tamaño debe ser mayor que cero.";
    }
    
    // Convertir a bytes - K, M o B (default K)
    std::string unitLower = CommandParser::toLower(unitStr);
    if (unitLower == "k") {
        size *= 1024;
    } else if (unitLower == "m") {
        size *= 1024 * 1024;
    } else if (unitLower == "b") {
        // Ya está en bytes, no hacer nada
    } else {
        return "Error: Unidad no válida. Use B, K o M.";
    }
    
    // Validar tipo
    char partType = CommandParser::toLower(typeStr)[0];
    if (partType != 'p' && partType != 'e' && partType != 'l') {
        return "Error: Tipo inválido. Use P, E o L.";
    }
    
    // Validar fit - debe ser BF, FF o WF
    std::string fitLower = CommandParser::toLower(fitStr);
    char fitChar = 'F';  // Por defecto First Fit
    
    if (fitLower == "bf" || fitLower == "b") {
        fitChar = 'B';
    } else if (fitLower == "ff" || fitLower == "f") {
        fitChar = 'F';
    } else if (fitLower == "wf" || fitLower == "w") {
        fitChar = 'W';
    } else {
        return "Error: Fit no válido. Use BF, FF o WF.";
    }
    
    // Leer MBR
    int diskSize = (int)DiskManager::getFileSize(path);
    if (diskSize <= 0) {
        return "Error: No se pudo obtener tamaño del disco.";
    }
    
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Verificar nombre único
    if (DiskManager::findPartitionByName(path, name) != -1) {
        return "Error: Partición con ese nombre ya existe.";
    }
    
    // Validar límites de particiones
    int primaryCount = 0;
    int extendedCount = 0;
    int freeSlot = -1;
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'N') {
            if (freeSlot == -1) freeSlot = i;
        } else {
            if (mbr.mbr_partitions[i].part_type == 'P') primaryCount++;
            else if (mbr.mbr_partitions[i].part_type == 'E') extendedCount++;
        }
    }
    
    // Validaciones específicas
    if (partType == 'p' || partType == 'e') {
        if (primaryCount + extendedCount >= 4) {
            return "Error: Ya existen 4 particiones primarias/extendidas. No se puede crear más.";
        }
        if (partType == 'e' && extendedCount > 0) {
            return "Error: Solo se permite una partición extendida por disco. Ya existe una.";
        }
        // Obtener slot libre 
        if (freeSlot == -1) {
            return "Error: No hay slots libres en la tabla de particiones.";
        }
    } else if (partType == 'l') {
        if (extendedCount == 0) {
            return "Error: Debe haber una partición extendida para crear particiones lógicas.";
        }
    }
    
    // Verificar espacio
    int freeSpace = DiskManager::getFreeSpace(path, diskSize);
    if (freeSpace < size) {
        return "Error: No hay espacio suficiente en el disco.";
    }
    
    // Crear partición primaria o extendida
    if (partType == 'p' || partType == 'e') {
        int startPos = DiskManager::calculatePartitionStart(path, size, fitChar, diskSize);
        if (startPos < 0) {
            return "Error: No se pudo encontrar espacio para la partición.";
        }
        
        Partition newPart;
        newPart.part_status = '0';
        newPart.part_type = (partType == 'p') ? 'P' : 'E';
        newPart.part_fit = fitChar;
        newPart.part_start = startPos;
        newPart.part_s = size;
        newPart.part_correlative = -1;
        std::strcpy(newPart.part_name, name.c_str());
        std::strcpy(newPart.part_id, "");
        
        mbr.mbr_partitions[freeSlot] = newPart;
        
        if (!DiskManager::writeMBR(path, mbr)) {
            return "Error: No se pudo escribir en el MBR.";
        }
        
        // Si es partición extendida, crear el primer EBR
        if (partType == 'e') {
            EBR ebr;
            ebr.part_mount = '0';
            ebr.part_fit = fitChar;
            ebr.part_start = -1;
            ebr.part_s = 0;
            ebr.part_next = -1;
            std::strcpy(ebr.part_name, "");
            
            if (!DiskManager::writeToDisk(path, newPart.part_start, (char*)&ebr, sizeof(EBR))) {
                return "Error: No se pudo crear EBR inicial para partición extendida.";
            }
        }
        
        return "Partición " + (partType == 'p' ? std::string("Primaria") : (partType == 'e' ? std::string("Extendida") : std::string("Lógica"))) + 
               " creada: " + name + " | Inicio: " + std::to_string(startPos) + " | Size: " + std::to_string(size);
    } else if (partType == 'l') {
        // Particiones lógicas: solo se permite crear si hay una extendida
        // Encontrar la partición extendida
        int extendedIndex = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E') {
                extendedIndex = i;
                break;
            }
        }
        
        if (extendedIndex == -1) {
            return "Error: No existe partición extendida.";
        }
        
        Partition& extPart = mbr.mbr_partitions[extendedIndex];
        
        // Leer el primer EBR para encontrar donde termina la última partición lógica
        EBR currentEBR;
        int ebrPosition = extPart.part_start;
        int lastEBRPosition = extPart.part_start;
        int usedSpace = sizeof(EBR);  // El primer EBR ya ocupa espacio
        int ebrCount = 0;
        
        // Recorrer la cadena de EBRs
        while (ebrCount < 100) {  // Protección contra bucles infinitos
            if (!DiskManager::readFromDisk(path, ebrPosition, (char*)&currentEBR, sizeof(EBR))) {
                return "Error: No se pudo leer EBR.";
            }
            
            if (currentEBR.part_start > 0) {
                usedSpace += sizeof(EBR) + currentEBR.part_s;
            }
            
            lastEBRPosition = ebrPosition;
            
            if (currentEBR.part_next == -1) {
                break;
            }
            
            ebrPosition = currentEBR.part_next;
            ebrCount++;
        }
        
        // Verificar si hay espacio
        if (usedSpace + sizeof(EBR) + size > extPart.part_s) {
            return "Error: No hay espacio suficiente en la partición extendida.";
        }
        
        // Calcular posición de inicio para la nueva partición lógica
        int logicalPartStart = extPart.part_start + usedSpace + sizeof(EBR);
        
        // Crear el nuevo EBR para la partición lógica
        EBR newLogicalEBR;
        newLogicalEBR.part_mount = '0';
        newLogicalEBR.part_fit = fitChar;
        newLogicalEBR.part_start = logicalPartStart;
        newLogicalEBR.part_s = size;
        newLogicalEBR.part_next = -1;
        std::strcpy(newLogicalEBR.part_name, name.c_str());
        
        // Actualizar el último EBR para apuntar al nuevo
        if (ebrCount > 0 && lastEBRPosition != extPart.part_start) {
            // Hay particiones lógicas previas
            EBR lastEBR;
            if (!DiskManager::readFromDisk(path, lastEBRPosition, (char*)&lastEBR, sizeof(EBR))) {
                return "Error: No se pudo leer EBR anterior.";
            }
            lastEBR.part_next = lastEBRPosition + sizeof(EBR) + lastEBR.part_s;
            
            if (!DiskManager::writeToDisk(path, lastEBRPosition, (char*)&lastEBR, sizeof(EBR))) {
                return "Error: No se pudo actualizar EBR anterior.";
            }
        } else {
            // Primera partición lógica - actualizar el EBR inicial
            currentEBR.part_next = extPart.part_start + sizeof(EBR);
            if (!DiskManager::writeToDisk(path, extPart.part_start, (char*)&currentEBR, sizeof(EBR))) {
                return "Error: No se pudo actualizar EBR inicial.";
            }
        }
        
        // Escribir el nuevo EBR
        int newEBRPosition = lastEBRPosition + sizeof(EBR) + (currentEBR.part_s > 0 ? currentEBR.part_s : 0);
        if (!DiskManager::writeToDisk(path, newEBRPosition, (char*)&newLogicalEBR, sizeof(EBR))) {
            return "Error: No se pudo escribir nuevo EBR para partición lógica.";
        }
        
        return "Partición Lógica creada: " + name + " | Start: " + std::to_string(logicalPartStart);
    }
}

// Implementación de fdisk -delete
std::string CommandHandler::cmdFdiskDelete(const std::string& path, const std::string& name, const std::string& deleteType) {
    // Validar tipo de eliminación
    std::string deleteTypeLower = CommandParser::toLower(deleteType);
    if (deleteTypeLower != "fast" && deleteTypeLower != "full") {
        return "Error: Tipo de eliminación inválido. Use 'fast' o 'full'.";
    }
    
    // Leer MBR
    int diskSize = (int)DiskManager::getFileSize(path);
    if (diskSize <= 0) {
        return "Error: No se pudo obtener tamaño del disco.";
    }
    
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Buscar la partición
    int partIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type != 'N' && 
            std::string(mbr.mbr_partitions[i].part_name) == name) {
            partIndex = i;
            break;
        }
    }
    
    if (partIndex == -1) {
        return "Error: No existe la partición con nombre '" + name + "'.";
    }
    
    Partition& partToDelete = mbr.mbr_partitions[partIndex];
    
    // Si es extendida, eliminar todas las lógicas
    if (partToDelete.part_type == 'E') {
        // Leer el primer EBR para encontrar todas las particiones lógicas
        EBR currentEBR;
        int ebrPosition = partToDelete.part_start;
        
        for (int i = 0; i < 100; i++) {  // Protección contra bucles infinitos
            if (!DiskManager::readFromDisk(path, ebrPosition, (char*)&currentEBR, sizeof(EBR))) {
                break;
            }
            
            if (deleteTypeLower == "full") {
                // Rellenar con \0 el área de la partición lógica
                if (currentEBR.part_start > 0 && currentEBR.part_s > 0) {
                    char* emptyData = new char[currentEBR.part_s]();
                    DiskManager::writeToDisk(path, currentEBR.part_start, emptyData, currentEBR.part_s);
                    delete[] emptyData;
                }
            }
            
            if (currentEBR.part_next == -1) break;
            ebrPosition = currentEBR.part_next;
        }
    }
    
    // Si es eliminación full, rellenar el espacio con \0
    if (deleteTypeLower == "full") {
        char* emptyData = new char[partToDelete.part_s]();
        if (!DiskManager::writeToDisk(path, partToDelete.part_start, emptyData, partToDelete.part_s)) {
            delete[] emptyData;
            return "Error: No se pudo limpiar el espacio de la partición.";
        }
        delete[] emptyData;
    }
    
    // Marcar partición como 'N' (vacía) en el MBR
    partToDelete.part_status = '0';
    partToDelete.part_type = 'N';
    partToDelete.part_fit = 'N';
    partToDelete.part_start = -1;
    partToDelete.part_s = 0;
    partToDelete.part_correlative = -1;
    std::strcpy(partToDelete.part_name, "");
    std::strcpy(partToDelete.part_id, "");
    
    // Escribir MBR actualizado
    if (!DiskManager::writeMBR(path, mbr)) {
        return "Error: No se pudo actualizar el MBR.";
    }
    
    return "Partición '" + name + "' eliminada correctamente (modo: " + deleteType + ").";
}

// Implementación de fdisk -add
std::string CommandHandler::cmdFdiskAdd(const std::string& path, const std::string& name, 
                                        const std::string& addStr, const std::map<std::string, std::string>& params) {
    // Convertir el valor de add a entero
    int addValue;
    try {
        addValue = std::stoi(addStr);
    } catch (...) {
        return "Error: Valor de -add inválido.";
    }
    
    // Obtener la unidad (puede ser B, K, M)
    std::string unitStr = CommandParser::getParameter(params, "unit", "K");
    std::string unitLower = CommandParser::toLower(unitStr);
    
    int unitMultiplier = 1024;  // K por defecto
    if (unitLower == "k") {
        unitMultiplier = 1024;
    } else if (unitLower == "m") {
        unitMultiplier = 1024 * 1024;
    } else if (unitLower == "b") {
        unitMultiplier = 1;
    } else {
        return "Error: Unidad no válida. Use B, K o M.";
    }
    
    // Convertir a bytes
    addValue *= unitMultiplier;
    
    // Leer MBR
    int diskSize = (int)DiskManager::getFileSize(path);
    if (diskSize <= 0) {
        return "Error: No se pudo obtener tamaño del disco.";
    }
    
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Buscar la partición
    int partIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type != 'N' && 
            std::string(mbr.mbr_partitions[i].part_name) == name) {
            partIndex = i;
            break;
        }
    }
    
    if (partIndex == -1) {
        return "Error: No existe la partición con nombre '" + name + "'.";
    }
    
    Partition& partToModify = mbr.mbr_partitions[partIndex];
    
    // Si es positivo, agregar espacio
    if (addValue > 0) {
        // Verificar que hay espacio libre después de la partición
        int spaceBefore = 0;
        int spaceAfter = 0;
        
        // Calcular espacio ocupado antes
        for (int i = 0; i < partIndex; i++) {
            if (mbr.mbr_partitions[i].part_type != 'N') {
                spaceBefore = std::max(spaceBefore, mbr.mbr_partitions[i].part_start + mbr.mbr_partitions[i].part_s);
            }
        }
        
        // Calcular espacio after
        int endOfCurrentPart = partToModify.part_start + partToModify.part_s;
        spaceAfter = diskSize - endOfCurrentPart;
        
        if (spaceAfter < addValue) {
            return "Error: No hay espacio suficiente después de la partición para agregar " + 
                   std::to_string(addValue) + " bytes. Disponible: " + std::to_string(spaceAfter);
        }
        
        partToModify.part_s += addValue;
    } 
    // Si es negativo, quitar espacio
    else if (addValue < 0) {
        int toRemove = -addValue;
        
        if (partToModify.part_s + addValue <= 0) {
            return "Error: No se puede quitar " + std::to_string(toRemove) + 
                   " bytes. La partición solo tiene " + std::to_string(partToModify.part_s) + " bytes.";
        }
        
        partToModify.part_s += addValue;  // addValue es negativo, así que suma resta
    }
    // Si es 0, no hacer nada
    else {
        return "Error: El valor de -add debe ser distinto de 0.";
    }
    
    // Escribir MBR actualizado
    if (!DiskManager::writeMBR(path, mbr)) {
        return "Error: No se pudo actualizar el MBR.";
    }
    
    if (addValue > 0) {
        return "Espacio agregado exitosamente a la partición '" + name + "'. Nuevo tamaño: " + 
               std::to_string(partToModify.part_s) + " bytes.";
    } else {
        return "Espacio removido exitosamente de la partición '" + name + "'. Nuevo tamaño: " + 
               std::to_string(partToModify.part_s) + " bytes.";
    }
}

std::string CommandHandler::cmdMount(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"path", "name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    std::string name = CommandParser::getParameter(params, "name");
    
    if (!DiskManager::fileExists(path)) {
        return "Error: El disco no existe.";
    }
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Buscar partición por nombre
    int partIndex = DiskManager::findPartitionByName(path, name);
    if (partIndex < 0) {
        return "Error: No se encontró la partición '" + name + "' en el disco.";
    }
    
    // Validar que sea primaria
    if (mbr.mbr_partitions[partIndex].part_type != 'P') {
        return "Error: Solo se pueden montar particiones primarias.";
    }
    
    // Validar que no esté ya montada (verificar clave disco:partición)
    std::string diskPartKey = path + ":" + name;
    if (mountedDiskPartitions.find(diskPartKey) != mountedDiskPartitions.end()) {
        return "Error: La partición '" + name + "' ya está montada.";
    }
    
    // Determinar número de partición y letra para este disco
    int partitionNumber = 1;
    char letter = 'A';
    
    // Si ya hay montajes de este disco, incrementar numero y mantener letra
    // Si es otro disco, incrementar letra y reiniciar numero
    if (diskMountStates.find(path) != diskMountStates.end()) {
        // Este disco ya tiene montajes
        partitionNumber = diskMountStates[path] + 1;
        letter = diskLetterStates[path];
    } else {
        // Primer montaje de este disco
        // Contar discos ya montados para determinar la letra
        char maxLetter = 'A';
        for (const auto& pair : diskLetterStates) {
            maxLetter = std::max(maxLetter, pair.second);
        }
        
        // Si ya hay otros discos montados, usar la siguiente letra
        if (diskLetterStates.size() > 0) {
            letter = maxLetter + 1;
        } else {
            letter = 'A';
        }
        partitionNumber = 1;
        diskLetterStates[path] = letter;
    }
    
    // Incrementar contador para este disco
    diskMountStates[path] = partitionNumber;
    diskLetterStates[path] = letter;
    
    // Generar ID con formato
    char idStr[10];
    sprintf(idStr, "06%d%c", partitionNumber, letter);
    
    // Actualizar partición en memoria (NO escribir a disco)
    mbr.mbr_partitions[partIndex].part_status = '1';
    mbr.mbr_partitions[partIndex].part_correlative = partitionNumber;
    std::strcpy(mbr.mbr_partitions[partIndex].part_id, idStr);
    
    // Guardar en particiones montadas
    mountedPartitions[std::string(idStr)] = path;
    // Guardar información de la partición para lookup rápido
    partitionInfo[std::string(idStr)] = std::make_pair(path, partIndex);
    // Guardar nombre de la partición
    partitionNames[std::string(idStr)] = name;
    // Registrar que esta partición ya está montada
    mountedDiskPartitions.insert(diskPartKey);
    
    // Actualizar Superblock
    Superblock sb;
    if (DiskManager::readSuperblock(path, mbr.mbr_partitions[partIndex].part_start, sb)) {
        sb.s_mnt_count++;
        sb.s_mtime = time(nullptr);
        DiskManager::writeSuperblock(path, mbr.mbr_partitions[partIndex].part_start, sb);
    }
    
    return "Partición montada con éxito. ID: " + std::string(idStr);
}

std::string CommandHandler::cmdUnmount(const std::map<std::string, std::string>& params) {
    // Validar parámetro obligatorio
    std::vector<std::string> mandatory = {"id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string id = CommandParser::getParameter(params, "id");
    std::transform(id.begin(), id.end(), id.begin(), ::toupper);
    
    // Buscar si la partición está montada
    if (mountedPartitions.find(id) == mountedPartitions.end()) {
        return "Error: No existe una partición montada con el ID '" + id + "'.";
    }
    
    // Obtener información de la partición
    std::string diskPath = mountedPartitions[id];
    auto partInfo = partitionInfo[id];
    int partIndex = partInfo.second;
    std::string partName = partitionNames[id];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Resetear el correlativo y estado de la partición
    mbr.mbr_partitions[partIndex].part_status = '0';
    mbr.mbr_partitions[partIndex].part_correlative = 0;  // Volver al estado inicial
    std::strcpy(mbr.mbr_partitions[partIndex].part_id, "");
    
    // Escribir MBR actualizado
    if (!DiskManager::writeMBR(diskPath, mbr)) {
        return "Error: No se pudo escribir el MBR.";
    }
    
    // Actualizar Superblock
    Superblock sb;
    if (DiskManager::readSuperblock(diskPath, mbr.mbr_partitions[partIndex].part_start, sb)) {
        sb.s_umtime = time(nullptr);
        DiskManager::writeSuperblock(diskPath, mbr.mbr_partitions[partIndex].part_start, sb);
    }
    
    // Remover de los mapas
    mountedPartitions.erase(id);
    partitionInfo.erase(id);
    partitionNames.erase(id);
    
    // Remover del conjunto de particiones montadas
    std::string diskPartKey = diskPath + ":" + partName;
    mountedDiskPartitions.erase(diskPartKey);
    
    // Si este disco no tiene más particiones montadas, limpiar referencias
    bool hasMountedPartitions = false;
    for (const auto& pair : partitionInfo) {
        if (pair.second.first == diskPath) {
            hasMountedPartitions = true;
            break;
        }
    }
    
    if (!hasMountedPartitions) {
        diskMountStates.erase(diskPath);
        diskLetterStates.erase(diskPath);
    }
    
    return "Partición con ID '" + id + "' desmontada exitosamente. Correlativo establecido a 0.";
}

std::string CommandHandler::cmdMounted(const std::map<std::string, std::string>& params) {
    if (mountedPartitions.empty()) {
        return "No hay particiones montadas.";
    }
    
    std::string result = "--- Particiones Montadas ---\n";
    for (const auto& pair : mountedPartitions) {
        std::string id = pair.first;
        std::string diskPath = pair.second;
        std::string partName = partitionNames[id];
        
        result += "ID: " + id + " | Nombre: " + partName + " | Disco: " + diskPath + "\n";
    }
    
    // Remover última línea en blanco
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

std::string CommandHandler::cmdMkfs(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string id = CommandParser::getParameter(params, "id");
    std::transform(id.begin(), id.end(), id.begin(), ::toupper);
    std::string typeStr = CommandParser::getParameter(params, "type", "full");
    std::string fsStr = CommandParser::getParameter(params, "fs", "2fs");  // Default: EXT2
    
    // Validar que type sea FULL 
    if (CommandParser::toLower(typeStr) != "full") {
        return "Error: Type debe ser 'full'. Valor recibido: " + typeStr;
    }
    
    // Validar filesystem type
    std::string fsLower = CommandParser::toLower(fsStr);
    int filesystemType = 2;  // Default: EXT2
    if (fsLower == "2fs" || fsLower == "2") {
        filesystemType = 2;
    } else if (fsLower == "3fs" || fsLower == "3") {
        filesystemType = 3;
    } else {
        return "Error: Filesystem inválido. Use '2fs' para EXT2 o '3fs' para EXT3.";
    }
    
    // Buscar partición montada
    if (mountedPartitions.find(id) == mountedPartitions.end()) {
        return "Error: Partición no montada. ID: " + id;
    }
    
    std::string diskPath = mountedPartitions[id];
    
    // Leer MBR para encontrar partición
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición por ID usando partitionInfo
    if (partitionInfo.find(id) == partitionInfo.end()) {
        return "Error: Partición no encontrada.";
    }
    int partIndex = partitionInfo[id].second;
    Partition* part = &mbr.mbr_partitions[partIndex];
    
    // Crear superblock
    Superblock sb;
    sb.s_filesystem_type = filesystemType;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = time(nullptr);
    sb.s_mnt_count = 0;
    sb.s_magic = 0xEF53;
    sb.s_inode_s = sizeof(Inodo);
    sb.s_block_s = 64;
    sb.s_firts_ino = 0;
    sb.s_first_blo = 0;
    
    int partStart = part->part_start;
    int partSize = part->part_s;
    
    // Calcular layout según el tipo de filesystem
    if (filesystemType == 2) {
        // EXT2: Sin journal
        DiskManager::EXT2Layout layout2 = DiskManager::calculateEXT2Layout(partSize);
        
        sb.s_inodes_count = layout2.num_inodes;
        sb.s_blocks_count = layout2.num_blocks;
        sb.s_free_blocks_count = layout2.num_blocks;
        sb.s_free_inodes_count = layout2.num_inodes;
        
        sb.s_bm_inode_start = layout2.inode_bitmap_start;
        sb.s_bm_block_start = layout2.block_bitmap_start;
        sb.s_inode_start = layout2.inode_table_start;
        sb.s_block_start = layout2.block_table_start;
        sb.s_journal_start = -1;  // EXT2 no tiene journal
        
        // Escribir superblock
        if (!DiskManager::writeToDisk(diskPath, partStart, (char*)&sb, sizeof(Superblock))) {
            return "Error: No se pudo escribir el superblock EXT2.";
        }
        
        // Inicializar bitmaps
        int bitmapInodeSize = (layout2.num_inodes + 7) / 8;
        int bitmapBlockSize = (layout2.num_blocks + 7) / 8;
        
        std::vector<char> zeroBitmap(std::max(bitmapInodeSize, bitmapBlockSize), 0);
        DiskManager::writeToDisk(diskPath, layout2.inode_bitmap_start, zeroBitmap.data(), bitmapInodeSize);
        DiskManager::writeToDisk(diskPath, layout2.block_bitmap_start, zeroBitmap.data(), bitmapBlockSize);
        
        // Crear raíz (inodo 0)
        Inodo rootIno;
        rootIno.i_uid = 0;
        rootIno.i_gid = 0;
        rootIno.i_s = 64;  // Tamaño del bloque
        rootIno.i_atime = time(nullptr);
        rootIno.i_ctime = time(nullptr);
        rootIno.i_mtime = time(nullptr);
        rootIno.i_type = 0;  // Directorio
        std::strcpy(rootIno.i_perm, "755");
        for (int i = 0; i < 15; i++) rootIno.i_block[i] = -1;
        
        // Asignar bloque para raíz
        int rootBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (rootBlock < 0) {
            return "Partición formateada pero no se pudo crear directorio raíz";
        }
        rootIno.i_block[0] = rootBlock;
        DiskManager::writeInodo(diskPath, partStart, 0, rootIno);
        
        // Crear contenido del directorio raíz con "." y ".."
        BlockFolder rootBlockContent;
        std::strcpy(rootBlockContent.b_content[0].b_name, ".");
        rootBlockContent.b_content[0].b_inodo = 0;
        std::strcpy(rootBlockContent.b_content[1].b_name, "..");
        rootBlockContent.b_content[1].b_inodo = 0;
        for (int i = 2; i < 4; i++) {
            std::strcpy(rootBlockContent.b_content[i].b_name, "");
            rootBlockContent.b_content[i].b_inodo = 0;
        }
        DiskManager::writeBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        
        // Crear archivo users.txt
        int usersInode = DiskManager::allocateInode(diskPath, partStart, sb);
        if (usersInode < 0) {
            return "Partición formateada pero no se pudo crear users.txt";
        }
        
        int usersBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (usersBlock < 0) {
            DiskManager::deallocateInode(diskPath, partStart, usersInode, sb);
            return "Partición formateada pero no se pudo crear users.txt";
        }
        
        // Crear contenido de users.txt
        std::string usersContent = "1,G,root\n1,U,root,root,123\n";
        BlockFile usersFileBlock;
        std::memset(usersFileBlock.b_content, 0, sizeof(usersFileBlock.b_content));
        std::memcpy(usersFileBlock.b_content, usersContent.c_str(), std::min((int)usersContent.length(), 64));
        
        // Escribir bloque del archivo
        DiskManager::writeBlock(diskPath, partStart, usersBlock, (char*)&usersFileBlock, sizeof(BlockFile));
        
        // Crear inodo para users.txt
        Inodo usersFileIno;
        usersFileIno.i_uid = 1;   // Usuario root (id=1)
        usersFileIno.i_gid = 1;   // Grupo root (id=1)
        usersFileIno.i_s = usersContent.length();
        usersFileIno.i_atime = time(nullptr);
        usersFileIno.i_ctime = time(nullptr);
        usersFileIno.i_mtime = time(nullptr);
        usersFileIno.i_type = 1;  // 1 = archivo
        std::strcpy(usersFileIno.i_perm, "644");
        for (int i = 1; i < 15; i++) usersFileIno.i_block[i] = -1;
        usersFileIno.i_block[0] = usersBlock;
        DiskManager::writeInodo(diskPath, partStart, usersInode, usersFileIno);
        
        // Agregar entrada users.txt al bloque raíz
        DiskManager::readBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        std::strcpy(rootBlockContent.b_content[2].b_name, "users.txt");
        rootBlockContent.b_content[2].b_inodo = usersInode;
        DiskManager::writeBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        
        // Actualizar superblock
        DiskManager::writeSuperblock(diskPath, partStart, sb);
        
        return "Partición formateada con EXT2 exitosamente. Inodos: " + std::to_string(layout2.num_inodes) + 
               ", Bloques: " + std::to_string(layout2.num_blocks);
        
    } else {
        // EXT3: Con journal
        DiskManager::EXT3Layout layout3 = DiskManager::calculateEXT3Layout(partSize);
        
        sb.s_inodes_count = layout3.num_inodes;
        sb.s_blocks_count = layout3.num_blocks;
        sb.s_free_blocks_count = layout3.num_blocks;
        sb.s_free_inodes_count = layout3.num_inodes;
        
        sb.s_bm_inode_start = layout3.inode_bitmap_start;
        sb.s_bm_block_start = layout3.block_bitmap_start;
        sb.s_inode_start = layout3.inode_table_start;
        sb.s_block_start = layout3.block_table_start;
        sb.s_journal_start = layout3.journal_start;  // EXT3 tiene journal
        
        // Escribir superblock
        if (!DiskManager::writeToDisk(diskPath, partStart, (char*)&sb, sizeof(Superblock))) {
            return "Error: No se pudo escribir el superblock EXT3.";
        }
        
        // Inicializar journal vacío
        Journal emptyJournal;
        emptyJournal.j_count = 0;
        for (int i = 0; i < 50; i++) {
            std::memset(emptyJournal.j_content[i].i_operation, 0, sizeof(emptyJournal.j_content[i].i_operation));
            std::memset(emptyJournal.j_content[i].i_path, 0, sizeof(emptyJournal.j_content[i].i_path));
            std::memset(emptyJournal.j_content[i].i_content, 0, sizeof(emptyJournal.j_content[i].i_content));
            emptyJournal.j_content[i].i_date = 0;
        }
        
        if (!DiskManager::writeJournal(diskPath, partStart, emptyJournal)) {
            return "Error: No se pudo inicializar el journal EXT3.";
        }
        
        // Inicializar bitmaps
        int bitmapInodeSize = (layout3.num_inodes + 7) / 8;
        int bitmapBlockSize = (layout3.num_blocks + 7) / 8;
        
        std::vector<char> zeroBitmap(std::max(bitmapInodeSize, bitmapBlockSize), 0);
        DiskManager::writeToDisk(diskPath, layout3.inode_bitmap_start, zeroBitmap.data(), bitmapInodeSize);
        DiskManager::writeToDisk(diskPath, layout3.block_bitmap_start, zeroBitmap.data(), bitmapBlockSize);
        
        // Crear raíz (inodo 0)
        Inodo rootIno;
        rootIno.i_uid = 0;
        rootIno.i_gid = 0;
        rootIno.i_s = 64;  // Tamaño del bloque
        rootIno.i_atime = time(nullptr);
        rootIno.i_ctime = time(nullptr);
        rootIno.i_mtime = time(nullptr);
        rootIno.i_type = 0;  // Directorio
        std::strcpy(rootIno.i_perm, "755");
        for (int i = 0; i < 15; i++) rootIno.i_block[i] = -1;
        
        // Asignar bloque para raíz
        int rootBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (rootBlock < 0) {
            return "Partición formateada pero no se pudo crear directorio raíz";
        }
        rootIno.i_block[0] = rootBlock;
        DiskManager::writeInodo(diskPath, partStart, 0, rootIno);
        
        // Crear contenido del directorio raíz con "." y ".."
        BlockFolder rootBlockContent;
        std::strcpy(rootBlockContent.b_content[0].b_name, ".");
        rootBlockContent.b_content[0].b_inodo = 0;
        std::strcpy(rootBlockContent.b_content[1].b_name, "..");
        rootBlockContent.b_content[1].b_inodo = 0;
        for (int i = 2; i < 4; i++) {
            std::strcpy(rootBlockContent.b_content[i].b_name, "");
            rootBlockContent.b_content[i].b_inodo = 0;
        }
        DiskManager::writeBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        
        // Crear archivo users.txt
        int usersInode = DiskManager::allocateInode(diskPath, partStart, sb);
        if (usersInode < 0) {
            return "Partición formateada pero no se pudo crear users.txt";
        }
        
        int usersBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (usersBlock < 0) {
            DiskManager::deallocateInode(diskPath, partStart, usersInode, sb);
            return "Partición formateada pero no se pudo crear users.txt";
        }
        
        // Crear contenido de users.txt
        std::string usersContent = "1,G,root\n1,U,root,root,123\n";
        BlockFile usersFileBlock;
        std::memset(usersFileBlock.b_content, 0, sizeof(usersFileBlock.b_content));
        std::memcpy(usersFileBlock.b_content, usersContent.c_str(), std::min((int)usersContent.length(), 64));
        
        // Escribir bloque del archivo
        DiskManager::writeBlock(diskPath, partStart, usersBlock, (char*)&usersFileBlock, sizeof(BlockFile));
        
        // Crear inodo para users.txt
        Inodo usersFileIno;
        usersFileIno.i_uid = 1;   // Usuario root (id=1)
        usersFileIno.i_gid = 1;   // Grupo root (id=1)
        usersFileIno.i_s = usersContent.length();
        usersFileIno.i_atime = time(nullptr);
        usersFileIno.i_ctime = time(nullptr);
        usersFileIno.i_mtime = time(nullptr);
        usersFileIno.i_type = 1;  // 1 = archivo
        std::strcpy(usersFileIno.i_perm, "644");
        for (int i = 1; i < 15; i++) usersFileIno.i_block[i] = -1;
        usersFileIno.i_block[0] = usersBlock;
        DiskManager::writeInodo(diskPath, partStart, usersInode, usersFileIno);
        
        // Agregar entrada users.txt al bloque raíz
        DiskManager::readBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        std::strcpy(rootBlockContent.b_content[2].b_name, "users.txt");
        rootBlockContent.b_content[2].b_inodo = usersInode;
        DiskManager::writeBlock(diskPath, partStart, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
        
        // Actualizar superblock
        DiskManager::writeSuperblock(diskPath, partStart, sb);
        
        return "Partición formateada con EXT3 exitosamente. Inodos: " + std::to_string(layout3.num_inodes) + 
               ", Bloques: " + std::to_string(layout3.num_blocks) + ", Journal inicializado.";
    }
}

std::string CommandHandler::cmdLogin(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"user", "pass", "id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    if (isLoggedIn) {
        return "Error: Ya existe una sesión activa (" + currentUser + ").";
    }
    
    std::string user = CommandParser::getParameter(params, "user");
    std::string pass = CommandParser::getParameter(params, "pass");
    std::string id = CommandParser::getParameter(params, "id");
    std::transform(id.begin(), id.end(), id.begin(), ::toupper);
    
    // Validar que la partición está montada
    if (mountedPartitions.find(id) == mountedPartitions.end()) {
        return "Error: Partición no montada con ID: " + id;
    }
    
    std::string diskPath = mountedPartitions[id];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando partitionInfo
    if (partitionInfo.find(id) == partitionInfo.end()) {
        return "Error: Partición no encontrada.";
    }
    int partIndex = partitionInfo[id].second;
    Partition* part = &mbr.mbr_partitions[partIndex];
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear users.txt y buscar usuario
    std::istringstream iss(usersContent);
    std::string line;
    bool userFound = false;
    bool passwordCorrect = false;
    int userUid = 0;
    std::string userGroup = "";

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Parsear línea: ID,Tipo,Grupo/Usuario,Usuario,Contraseña
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }

        // Solo procesar líneas de usuario (tipo U)
        if (fields.size() >= 5 && fields[1] == "U") {
            if (fields[3] == user) {
                userFound = true;
                if (fields[4] == pass) {
                    passwordCorrect = true;
                    userUid = std::stoi(fields[0]);
                    userGroup = fields[2];
                    break;
                }
            }
        }
    }

    if (!userFound) {
        return "Error: El usuario '" + user + "' no existe.";
    }

    if (!passwordCorrect) {
        return "Error: Contraseña incorrecta.";
    }

    // Buscar GID del grupo
    std::istringstream iss2(usersContent);
    int userGid = 1; // Default grupo root
    while (std::getline(iss2, line)) {
        if (line.empty()) continue;

        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }

        // Buscar grupo
        if (fields.size() >= 3 && fields[1] == "G" && fields[2] == userGroup) {
            userGid = std::stoi(fields[0]);
            break;
        }
    }

    // Todos los datos válidos, establecer sesión
    currentUser = user;
    currentPartitionId = id;
    currentUserUid = userUid;
    currentUserGid = userGid;
    isLoggedIn = true;

    return "Inicio de sesión exitoso. Bienvenido " + currentUser + " (UID: " + std::to_string(userUid) + ")";
}

std::string CommandHandler::cmdLogout(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: No hay sesión activa.";
    }
    
    std::string user = currentUser;
    currentUser = "";
    currentPartitionId = "";
    currentUserUid = 0;
    currentUserGid = 0;
    isLoggedIn = false;
    
    return "Sesión cerrada: " + user;
}

std::string CommandHandler::cmdCat(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: No hay partición montada.";
    }
    
    std::string result = "";
    int fileNum = 1;
    
    while (true) {
        std::string fileParam = "file" + std::to_string(fileNum);
        if (params.find(fileParam) == params.end()) {
            break;
        }
        
        std::string filePath = CommandParser::getParameter(params, fileParam);
        if (filePath.empty()) {
            return "Error: Archivo " + std::to_string(fileNum) + " no especificado.";
        }
        
        // Buscar partición montada
        std::string diskPath = mountedPartitions[currentPartitionId];
        
        // Leer MBR
        MBR mbr;
        if (!DiskManager::readMBR(diskPath, mbr)) {
            return "Error: No se pudo leer el MBR.";
        }
        
        // Encontrar partición usando helper
        Partition* part = findPartitionById(currentPartitionId, mbr);
        if (part == nullptr) {
            return "Error: Partición no encontrada.";
        }
        
        // Leer superblock
        Superblock sb;
        if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
            return "Error: No se pudo leer el superblock.";
        }
        
        // Extraer directorio y nombre del archivo
        std::string dirPath = "/";
        std::string fileName = filePath;
        
        size_t lastSlash = filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            dirPath = filePath.substr(0, lastSlash);
            if (dirPath.empty()) dirPath = "/";
            fileName = filePath.substr(lastSlash + 1);
        }
        
        // Obtener inode del directorio padre
        Inodo parentInoData;
        int parentInoNum = 0;
        
        if (dirPath != "/") {
            char typeCheck;
            parentInoNum = getInodeFromPath(diskPath, part->part_start, dirPath, parentInoData, typeCheck);
            if (parentInoNum < 0) {
                return "Error: El directorio '" + dirPath + "' no existe.";
            }
            // Leer el inodo correcto del directorio usando el número retornado
            if (!DiskManager::readInodo(diskPath, part->part_start, parentInoNum, parentInoData)) {
                return "Error: No se pudo leer el inodo del directorio.";
            }
        } else {
            // Leer inodo raíz
            if (!DiskManager::readInodo(diskPath, part->part_start, 0, parentInoData)) {
                return "Error: No se pudo leer el inodo raíz.";
            }
        }
        
        // Buscar archivo en los bloques del directorio (soporta múltiples)
        int fileInode = -1;
        for (int blockIdx = 0; blockIdx < 15 && parentInoData.i_block[blockIdx] != -1; blockIdx++) {
            BlockFolder dirBlock;
            if (!DiskManager::readBlock(diskPath, part->part_start, parentInoData.i_block[blockIdx], 
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
        if (!DiskManager::readInodo(diskPath, part->part_start, fileInode, fileIno)) {
            return "Error: No se pudo leer el inodo del archivo.";
        }
        
        if (fileIno.i_type != 1) {
            return "Error: '" + fileName + "' no es un archivo.";
        }
        
        // Validar permisos de lectura (root siempre tiene acceso)
        if (currentUserUid != 0) {  // Si no es root
            int permNum = std::stoi(std::string(fileIno.i_perm));
            int ownerPerm = (permNum / 100) % 10;  // Primer dígito
            int groupPerm = (permNum / 10) % 10;   // Segundo dígito
            int otherPerm = permNum % 10;          // Tercer dígito

            // Determinar qué permiso usar según UID/GID
            int effectivePerm = otherPerm;  // Por defecto, otros
            if (fileIno.i_uid == currentUserUid) {
                // Usuario es propietario
                effectivePerm = ownerPerm;
            } else if (fileIno.i_gid == currentUserGid) {
                // Usuario pertenece al grupo
                effectivePerm = groupPerm;
            }

            // Validar si tiene permiso de lectura (bit 4)
            if ((effectivePerm & 4) == 0) {
                return "Error: Permiso denegado. No tiene acceso de lectura para '" + fileName + "'.";
            }
        }
        
        // Leer contenido del archivo
        for (int i = 0; i < 12 && fileIno.i_block[i] >= 0; i++) {
            BlockFile block;
            int blockSize = std::min(64, fileIno.i_s - (i * 64));
            if (!DiskManager::readBlock(diskPath, part->part_start, fileIno.i_block[i], 
                                    (char*)&block, blockSize)) {
                return "Error: No se pudo leer bloque del archivo.";
            }
            result.append(block.b_content, blockSize);
        }
        
        if (fileNum < 2) {
            result += "\n";
        } else {
            result += "\n";
        }
        fileNum++;
    }
    
    if (fileNum == 1) {
        return "Error: No se especificaron archivos.";
    }
    
    // Remover última línea en blanco si existe
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

std::string CommandHandler::cmdMkgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root (UID = 1)
    if (currentUser != "root") {
        return "Error: Solo root puede crear grupos.";
    }
    
    std::vector<std::string> mandatory = {"name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string groupName = CommandParser::getParameter(params, "name");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar si el grupo ya existe
    std::istringstream iss(usersContent);
    std::string line;
    int maxId = 1;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            // Actualizar maxId
            try {
                int id = std::stoi(fields[0]);
                if (id > maxId) maxId = id;
            } catch (...) {}
            
            // Verificar si es el grupo que intentamos crear (solo si está activo, id > 0)
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == groupName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Solo si está activo (no eliminado)
                        return "Error: El grupo '" + groupName + "' ya existe.";
                    }
                } catch (...) {}
            }
        }
    }
    
    // Crear nueva línea para el grupo
    int newGroupId = maxId + 1;
    std::string newGroupLine = std::to_string(newGroupId) + ",G," + groupName + "\n";
    usersContent += newGroupLine;
    
    // Escribir nuevo contenido a users.txt
    // Actualizar tamaño del inodo
    usersIno.i_s = usersContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < usersContent.length()) {
        int blockSize = std::min(64, (int)(usersContent.length() - contentOffset));
        
        // Si el bloque no está asignado, allocarlo
        if (usersIno.i_block[blockIndex] < 0) {
            int newBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
            if (newBlock < 0) {
                return "Error: No hay bloques libres para escribir users.txt.";
            }
            usersIno.i_block[blockIndex] = newBlock;
        }
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, usersContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar superblock después de allocar bloques
    if (!DiskManager::writeSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo actualizar el superblock.";
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Grupo creado exitosamente: " + groupName + " (ID: " + std::to_string(newGroupId) + ")";
}

std::string CommandHandler::cmdRmgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede eliminar grupos.";
    }
    
    std::vector<std::string> mandatory = {"name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string groupName = CommandParser::getParameter(params, "name");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar el grupo a eliminar
    std::istringstream iss(usersContent);
    std::string line;
    std::string newContent = "";
    bool groupFound = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Verificar si es el grupo que queremos eliminar
        if (fields.size() >= 3 && fields[1] == "G" && fields[2] == groupName) {
            groupFound = true;
            // Marcar como eliminado
            newContent += "0,G," + groupName + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    if (!groupFound) {
        return "Error: El grupo '" + groupName + "' no existe.";
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() != '\n') {
        newContent += '\n';
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Grupo eliminado exitosamente: " + groupName;
}

std::string CommandHandler::cmdMkusr(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede crear usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user", "pass", "grp"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    std::string password = CommandParser::getParameter(params, "pass");
    std::string groupName = CommandParser::getParameter(params, "grp");
    
    // Validar longitud de parámetros
    if (userName.length() > 10) {
        return "Error: El nombre de usuario no puede exceder 10 caracteres.";
    }
    
    if (password.length() > 10) {
        return "Error: La contraseña no puede exceder 10 caracteres.";
    }
    
    if (groupName.length() > 10) {
        return "Error: El nombre del grupo no puede exceder 10 caracteres.";
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y validar
    std::istringstream iss(usersContent);
    std::string line;
    int maxId = 1;
    bool userExists = false;
    bool groupExists = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            try {
                int id = std::stoi(fields[0]);
                if (id > maxId) maxId = id;
            } catch (...) {}
            
            // Verificar si el usuario ya existe (solo si está activo, id > 0)
            if (fields[1] == "U" && fields.size() >= 4 && fields[3] == userName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Solo si está activo (no eliminado)
                        userExists = true;
                    }
                } catch (...) {}
            }
            
            // Verificar si el grupo existe y está activo
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == groupName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo (no eliminado)
                        groupExists = true;
                    }
                } catch (...) {}
            }
        }
    }
    
    if (userExists) {
        return "Error: El usuario '" + userName + "' ya existe.";
    }
    
    if (!groupExists) {
        return "Error: El grupo '" + groupName + "' no existe.";
    }
    
    // Crear nueva línea para el usuario
    int newUserId = maxId + 1;
    std::string newUserLine = std::to_string(newUserId) + ",U," + groupName + "," + userName + "," + password + "\n";
    usersContent += newUserLine;
    
    // Actualizar tamaño del inodo
    usersIno.i_s = usersContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < usersContent.length()) {
        int blockSize = std::min(64, (int)(usersContent.length() - contentOffset));
        
        // Si el bloque no está asignado, allocarlo
        if (usersIno.i_block[blockIndex] < 0) {
            int newBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
            if (newBlock < 0) {
                return "Error: No hay bloques libres para escribir users.txt.";
            }
            usersIno.i_block[blockIndex] = newBlock;
        }
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, usersContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar superblock después de allocar bloques
    if (!DiskManager::writeSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo actualizar el superblock.";
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Usuario creado exitosamente: " + userName + " (UID: " + std::to_string(newUserId) + ") en grupo " + groupName;
}

std::string CommandHandler::cmdRmusr(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede eliminar usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar el usuario a eliminar
    std::istringstream iss(usersContent);
    std::string line;
    std::string newContent = "";
    bool userFound = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Verificar si es el usuario que queremos eliminar
        if (fields.size() >= 4 && fields[1] == "U" && fields[3] == userName) {
            userFound = true;
            // Marcar como eliminado
            // Formato: 0,U,Grupo,Usuario,Contraseña
            newContent += "0,U," + fields[2] + "," + fields[3] + "," + fields[4] + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    if (!userFound) {
        return "Error: El usuario '" + userName + "' no existe.";
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() == '\n') {
        newContent.pop_back();
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Usuario eliminado exitosamente: " + userName;
}

std::string CommandHandler::cmdChgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede cambiar grupos de usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user", "grp"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    std::string newGroupName = CommandParser::getParameter(params, "grp");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y validar
    std::istringstream iss(usersContent);
    std::string line;
    bool userFound = false;
    bool groupExists = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            // Verificar si el usuario existe y está activo
            if (fields[1] == "U" && fields.size() >= 4 && fields[3] == userName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo
                        userFound = true;
                    }
                } catch (...) {}
            }
            
            // Verificar si el nuevo grupo existe y está activo
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == newGroupName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo
                        groupExists = true;
                    }
                } catch (...) {}
            }
        }
    }
    
    if (!userFound) {
        return "Error: El usuario '" + userName + "' no existe.";
    }
    
    if (!groupExists) {
        return "Error: El grupo '" + newGroupName + "' no existe.";
    }
    
    // Ahora actualizar el grupo del usuario
    iss.clear();
    iss.seekg(0);
    usersContent = "";
    iss.str(usersContent); 
    
    // Releer users.txt para actualizar
    usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Procesar línea por línea y actualizar el grupo del usuario
    std::istringstream issUpdate(usersContent);
    std::string newContent = "";
    
    while (std::getline(issUpdate, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Si es el usuario que queremos actualizar
        if (fields.size() >= 5 && fields[1] == "U" && fields[3] == userName) {
            // Formato: ID,U,NuevoGrupo,Usuario,Contraseña
            newContent += fields[0] + ",U," + newGroupName + "," + fields[3] + "," + fields[4] + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() == '\n') {
        newContent.pop_back();
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Grupo de '" + userName + "' cambiado a '" + newGroupName + "' exitosamente.";
}

std::string CommandHandler::cmdMkdir(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Validar parámetro -path obligatorio
    std::string dirPath = CommandParser::getParameter(params, "path");
    if (dirPath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    // Validar parámetro -p (no debe tener valor)
    bool createParents = CommandParser::hasParameter(params, "p");
    
    // Parsear ruta para obtener nombre de carpeta
    std::string folderName;
    std::string parentPath = "";
    
    size_t lastSlash = dirPath.find_last_of("/");
    if (lastSlash != std::string::npos && lastSlash > 0) {
        parentPath = dirPath.substr(0, lastSlash);
        folderName = dirPath.substr(lastSlash + 1);
    } else if (lastSlash == std::string::npos) {
        folderName = dirPath;
        parentPath = "/";
    } else {
        folderName = dirPath;
        parentPath = "/";
    }
    
    // Si parentPath es vacío, es raíz
    if (parentPath.empty()) {
        parentPath = "/";
    }

    // El filesystem almacena nombres de hasta 11 chars en b_name[12]
    if (folderName.size() > 11) {
        folderName = folderName.substr(0, 11);
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // PASO 2: Obtener el directorio padre correcto
    int parentInode = 0;
    Inodo parentInoData;
    BlockFolder parentBlock;
    
    if (parentPath == "/") {
        // Caso especial: crear en raíz
        parentInode = 0;
        if (!DiskManager::readInodo(diskPath, part->part_start, 0, parentInoData)) {
            return "Error: No se pudo leer inodo raíz.";
        }
        if (!DiskManager::readBlock(diskPath, part->part_start, parentInoData.i_block[0], (char*)&parentBlock, sizeof(BlockFolder))) {
            return "Error: No se pudo leer bloque raíz.";
        }
    } else {
        // Obtener el inodo y bloque del directorio padre
        char type;
        parentInode = getInodeFromPath(diskPath, part->part_start, parentPath, parentInoData, type);
        
        if (parentInode < 0) {
            if (createParents) {
                if (!createParentDirs(diskPath, part->part_start, sb.s_inode_start, dirPath)) {
                    return "Error: No se pudieron crear las carpetas padres.";
                }
                return "Carpeta creada exitosamente de forma recursiva: " + folderName;
            } else {
                return "Error: Las carpetas padres no existen. Use -p para crearlas.";
            }
        }
        
        // Leer bloque del directorio padre
        if (!DiskManager::readBlock(diskPath, part->part_start, parentInoData.i_block[0], (char*)&parentBlock, sizeof(BlockFolder))) {
            return "Error: No se pudo leer el bloque del directorio padre.";
        }
    }
    
    // PASO 3: Verificar si la carpeta ya existe en el directorio padre
    int startSlot = (parentInode == 0) ? 0 : 2;
    for (int i = 0; i < 4; i++) {
        if (parentBlock.b_content[i].b_inodo != 0) {
            std::string existingName(parentBlock.b_content[i].b_name, sizeof(parentBlock.b_content[i].b_name));
            size_t nullPos = existingName.find('\0');
            if (nullPos != std::string::npos) {
                existingName = existingName.substr(0, nullPos);
            }
            if (existingName == folderName) {
                return "Error: La carpeta '" + folderName + "' ya existe.";
            }
        }
    }
    
    // Asignar nuevo inodo
    int newInode = DiskManager::allocateInode(diskPath, part->part_start, sb);
    if (newInode < 0) {
        return "Error: No hay inodos disponibles.";
    }
    
    // Asignar bloque al directorio
    int dirBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
    if (dirBlock < 0) {
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No hay bloques disponibles.";
    }
    
    // Obtener UID/GID del usuario actual
    int userUid = (currentUser == "root") ? 1 : 2;
    int userGid = 1;  // Grupo por defecto
    
    // Crear inodo del directorio
    Inodo newDirIno;
    newDirIno.i_uid = userUid;
    newDirIno.i_gid = userGid;
    newDirIno.i_s = 0;
    newDirIno.i_atime = time(nullptr);
    newDirIno.i_ctime = time(nullptr);
    newDirIno.i_mtime = time(nullptr);
    newDirIno.i_type = 0;  // 0 = directorio
    std::strncpy(newDirIno.i_perm, "755", 3);  // rwxr-xr-x
    
    for (int i = 0; i < 15; i++) {
        newDirIno.i_block[i] = (i == 0) ? dirBlock : -1;
    }
    
    // Inicializar bloque del directorio vacío
    BlockFolder newDirBlock;
    memset(&newDirBlock, 0, sizeof(BlockFolder));
    
    // Escribir bloque del directorio
    if (!DiskManager::writeBlock(diskPath, part->part_start, dirBlock, (char*)&newDirBlock, sizeof(BlockFolder))) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el bloque del directorio.";
    }
    
    // Escribir inodo
    if (!DiskManager::writeInodo(diskPath, part->part_start, newInode, newDirIno)) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el inodo.";
    }
    
    // PASO 4: Agregar entrada de la nueva carpeta en el directorio padre
    bool added = false;
    for (int j = 0; j < 15 && parentInoData.i_block[j] != -1; j++) {
        BlockFolder folderBlock;
        if (!DiskManager::readBlock(diskPath, part->part_start, parentInoData.i_block[j], (char*)&folderBlock, sizeof(BlockFolder))) {
            continue;
        }

        for (int k = startSlot; k < 4; k++) {
            if (folderBlock.b_content[k].b_inodo == 0) {
                std::strncpy(folderBlock.b_content[k].b_name, folderName.c_str(), 11);
                folderBlock.b_content[k].b_name[11] = '\0';
                folderBlock.b_content[k].b_inodo = newInode;

                if (!DiskManager::writeBlock(diskPath, part->part_start, parentInoData.i_block[j], (char*)&folderBlock, sizeof(BlockFolder))) {
                    DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
                    DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
                    return "Error: No se pudo escribir el bloque del directorio padre.";
                }

                added = true;
                break;
            }
        }

        if (added) {
            break;
        }
    }

    if (!added) {
        int newBlockForParent = DiskManager::allocateBlock(diskPath, part->part_start, sb);
        if (newBlockForParent < 0) {
            DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No hay espacio en el directorio padre.";
        }

        bool linked = false;
        for (int j = 0; j < 15; j++) {
            if (parentInoData.i_block[j] == -1) {
                parentInoData.i_block[j] = newBlockForParent;
                linked = true;
                break;
            }
        }

        if (!linked) {
            DiskManager::deallocateBlock(diskPath, part->part_start, newBlockForParent, sb);
            DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: El directorio padre está lleno.";
        }

        BlockFolder newParentBlock;
        memset(&newParentBlock, 0, sizeof(BlockFolder));
        std::strncpy(newParentBlock.b_content[0].b_name, folderName.c_str(), 11);
        newParentBlock.b_content[0].b_name[11] = '\0';
        newParentBlock.b_content[0].b_inodo = newInode;

        if (!DiskManager::writeBlock(diskPath, part->part_start, newBlockForParent, (char*)&newParentBlock, sizeof(BlockFolder))) {
            DiskManager::deallocateBlock(diskPath, part->part_start, newBlockForParent, sb);
            DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo escribir el bloque del directorio padre.";
        }

        if (!DiskManager::writeInodo(diskPath, part->part_start, parentInode, parentInoData)) {
            DiskManager::deallocateBlock(diskPath, part->part_start, newBlockForParent, sb);
            DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo actualizar el inodo del directorio padre.";
        }

        added = true;
    }

    if (!added) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No hay espacio en el directorio padre.";
    }
    
    // Actualizar superblock
    DiskManager::writeSuperblock(diskPath, part->part_start, sb);
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "mkdir", 9);
        strncpy(info.i_path, folderName.c_str(), 31);
        strncpy(info.i_content, "Carpeta creada", 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, part->part_start, info);
    }
    
    return "Carpeta creada exitosamente: " + folderName + " (Inodo: " + std::to_string(newInode) + ")";
}

std::string CommandHandler::cmdMkfile(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Validar parámetro -path obligatorio
    std::string filePath = CommandParser::getParameter(params, "path");
    if (filePath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    // Validar parámetro -r 
    bool createDirs = CommandParser::hasParameter(params, "r");
    
    // Validar parámetro -size
    std::string sizeStr = CommandParser::getParameter(params, "size", "0");
    int fileSize = 0;
    try {
        fileSize = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Parámetro -size inválido.";
    }
    
    if (fileSize < 0) {
        return "Error: El tamaño del archivo no puede ser negativo.";
    }
    
    // Validar parámetro -cont
    std::string contPath = CommandParser::getParameter(params, "cont", "");
    std::string fileContent = "";
    
    if (!contPath.empty()) {
        // Intentar leer archivo del sistema
        std::ifstream contFile(contPath, std::ios::binary);
        if (!contFile) {
            return "Error: No se pudo leer el archivo: " + contPath;
        }
        
        // Leer todo el contenido
        contFile.seekg(0, std::ios::end);
        size_t contSize = contFile.tellg();
        contFile.seekg(0, std::ios::beg);
        
        fileContent.resize(contSize);
        contFile.read(&fileContent[0], contSize);
        contFile.close();
        
        fileSize = contSize;  // -cont tiene prioridad sobre -size
    }
    
    // Si no hay contenido externo, generar contenido con patrón 0-9
    if (fileContent.empty() && fileSize > 0) {
        for (int i = 0; i < fileSize; i++) {
            fileContent += char('0' + (i % 10));
        }
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando helper
    Partition* part = findPartitionById(currentPartitionId, mbr);
    if (part == nullptr) {
        return "Error: Partición no encontrada.";    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Parsear ruta para obtener padre y nombre
    std::string fileName;
    std::string dirPath = "";
    
    size_t lastSlash = filePath.find_last_of("/");
    if (lastSlash != std::string::npos) {
        dirPath = filePath.substr(0, lastSlash);
        fileName = filePath.substr(lastSlash + 1);
    } else {
        fileName = filePath;
        dirPath = "/";
    }
    
    // Si dirPath es vacío, es raíz
    if (dirPath.empty()) {
        dirPath = "/";
    }

    // El filesystem almacena nombres de hasta 11 chars en b_name[12]
    if (fileName.size() > 11) {
        fileName = fileName.substr(0, 11);
    }
    
    // Crear carpetas padres si es necesario
    if (dirPath != "/" && createDirs) {
        if (!createParentDirs(diskPath, part->part_start, sb.s_inode_start, dirPath)) {
            return "Error: No se pudieron crear las carpetas padres.";
        }
    }
    
    // Obtener el inodo del directorio padre ANTES de procesar el archivo
    Inodo parentInoData;
    int parentInoNum = 0;  // Por defecto raíz
    
    if (dirPath != "/") {
        char typeCheck;
        parentInoNum = getInodeFromPath(diskPath, part->part_start, dirPath, parentInoData, typeCheck);
        if (parentInoNum < 0) {
            return "Error: Las carpetas padres no existen. Use -r para crearlas.";
        }
        // Leer el inodo correcto del directorio usando el número retornado
        if (!DiskManager::readInodo(diskPath, part->part_start, parentInoNum, parentInoData)) {
            return "Error: No se pudo leer el inodo del directorio.";
        }
    } else {
        // Leer inodo raíz
        if (!DiskManager::readInodo(diskPath, part->part_start, 0, parentInoData)) {
            return "Error: No se pudo leer el inodo raíz.";
        }
    }
    
    // Asignar nuevo inodo para el archivo
    int newInode = DiskManager::allocateInode(diskPath, part->part_start, sb);
    if (newInode < 0) {
        return "Error: No hay inodos disponibles.";
    }
    
    // Calcular bloques necesarios 
    int blocksNeeded = (fileSize + 63) / 64;
    if (blocksNeeded > 12) {
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: Archivo muy grande (máximo 768 bytes).";
    }
    
    // Asignar bloques
    std::vector<int> fileBlocks;
    for (int i = 0; i < blocksNeeded; i++) {
        int block = DiskManager::allocateBlock(diskPath, part->part_start, sb);
        if (block < 0) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No hay bloques disponibles.";
        }
        fileBlocks.push_back(block);
    }
    
    // Obtener UID/GID del usuario actual 
    int userUid = (currentUser == "root") ? 1 : 2;
    int userGid = 1;  // Grupo por defecto
    
    // Crear inodo del archivo
    Inodo fileIno;
    fileIno.i_uid = userUid;
    fileIno.i_gid = userGid;
    fileIno.i_s = fileSize;
    fileIno.i_atime = time(nullptr);
    fileIno.i_ctime = time(nullptr);
    fileIno.i_mtime = time(nullptr);
    fileIno.i_type = 1;  // 1 = archivo
    std::strncpy(fileIno.i_perm, "664", 3);  // rw-rw-r--
    
    for (int i = 0; i < 15; i++) {
        fileIno.i_block[i] = (i < blocksNeeded) ? fileBlocks[i] : -1;
    }
    
    // Escribir bloques de archivo
    int bytesWritten = 0;
    for (int i = 0; i < blocksNeeded; i++) {
        BlockFile fileBlock;
        memset(&fileBlock, 0, sizeof(BlockFile));
        
        int bytesToWrite = std::min(64, fileSize - bytesWritten);
        std::memcpy(fileBlock.b_content, fileContent.c_str() + bytesWritten, bytesToWrite);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, fileBlocks[i], (char*)&fileBlock, sizeof(BlockFile))) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo escribir bloque del archivo.";
        }
        
        bytesWritten += bytesToWrite;
    }
    
    // Escribir inodo del archivo
    if (!DiskManager::writeInodo(diskPath, part->part_start, newInode, fileIno)) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el inodo.";
    }
    
    // Buscar espacio en los bloques del directorio (soporta múltiples)
    bool added = false;
    int blockToWrite = -1;
    int slotIndex = -1;
    BlockFolder parentBlock;
    
    for (int blockIdx = 0; blockIdx < 15 && parentInoData.i_block[blockIdx] != -1; blockIdx++) {
        BlockFolder dirBlock;
        if (!DiskManager::readBlock(diskPath, part->part_start, parentInoData.i_block[blockIdx], 
                                (char*)&dirBlock, sizeof(BlockFolder))) {
            continue;
        }
        
        // Verificar si el archivo ya existe en este bloque
        for (int j = 0; j < 4; j++) {
            if (dirBlock.b_content[j].b_inodo != 0 && 
                std::string(dirBlock.b_content[j].b_name) == fileName) {
                for (int b : fileBlocks) {
                    DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
                }
                DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
                return "Error: El archivo '" + fileName + "' ya existe.";
            }
        }
        
        // Buscar espacio vacío en este bloque
        if (!added) {
            for (int j = 0; j < 4; j++) {
                if (dirBlock.b_content[j].b_inodo == 0) {
                    blockToWrite = parentInoData.i_block[blockIdx];
                    slotIndex = j;
                    added = true;
                    break;
                }
            }
        }
    }
    
    // Si no hay espacio en bloques existentes, crear uno nuevo
    if (!added) {
        int newBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
        if (newBlock < 0) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No hay bloques disponibles para el directorio.";
        }
        
        // Encontrar el siguiente apuntador libre en el inodo del directorio
        bool blockAssigned = false;
        for (int i = 0; i < 15; i++) {
            if (parentInoData.i_block[i] == -1) {
                parentInoData.i_block[i] = newBlock;
                blockAssigned = true;
                break;
            }
        }
        
        if (!blockAssigned) {
            DiskManager::deallocateBlock(diskPath, part->part_start, newBlock, sb);
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: El directorio padre está lleno.";
        }
        
        // Inicializar el nuevo bloque
        BlockFolder newDirBlock;
        memset(&newDirBlock, 0, sizeof(BlockFolder));
        
        // Agregar la entrada en el nuevo bloque
        std::strncpy(newDirBlock.b_content[0].b_name, fileName.c_str(), 11);
        newDirBlock.b_content[0].b_inodo = newInode;
        
        blockToWrite = newBlock;
        parentBlock = newDirBlock;
    } else {
        // Leer el bloque donde vamos a agregar
        if (!DiskManager::readBlock(diskPath, part->part_start, blockToWrite, (char*)&parentBlock, sizeof(BlockFolder))) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo leer el bloque del directorio.";
        }
        
        // Agregar la entrada
        std::strncpy(parentBlock.b_content[slotIndex].b_name, fileName.c_str(), 11);
        parentBlock.b_content[slotIndex].b_inodo = newInode;
    }
    
    // Escribir el bloque actualizado
    if (!DiskManager::writeBlock(diskPath, part->part_start, blockToWrite, (char*)&parentBlock, sizeof(BlockFolder))) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el bloque del directorio.";
    }
    
    // Si se asignó un nuevo bloque, actualizar el inodo del directorio padre
    if (slotIndex == -1) {
        if (!DiskManager::writeInodo(diskPath, part->part_start, parentInoNum, parentInoData)) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo actualizar el inodo del directorio padre.";
        }
    }
    
    // Escribir superblock actualizado
    if (!DiskManager::writeSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo actualizar el superblock.";
    }
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "mkfile", 9);
        strncpy(info.i_path, fileName.c_str(), 31);
        strncpy(info.i_content, "Archivo creado", 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, part->part_start, info);
    }
    
    return "Archivo creado exitosamente: " + fileName + " (Inodo: " + std::to_string(newInode) + ")";
}

std::string CommandHandler::cmdRep(const std::map<std::string, std::string>& params) {
    // Validar parámetros obligatorios
    std::string repName = CommandParser::getParameter(params, "name");
    if (repName.empty()) {
        return "Error: Parámetro obligatorio faltante: -name";
    }
    
    std::string repPath = CommandParser::getParameter(params, "path");
    if (repPath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    std::string partitionId = CommandParser::getParameter(params, "id");
    if (partitionId.empty()) {
        return "Error: Parámetro obligatorio faltante: -id";
    }
    
    // Validar tipo de reporte
    std::vector<std::string> validReports = {"mbr", "disk", "inode", "block", "bm_inode", "bm_block", "tree", "sb", "file", "ls", "ebr"};
    if (std::find(validReports.begin(), validReports.end(), repName) == validReports.end()) {
        return "Error: Tipo de reporte inválido: " + repName;
    }
    
    // Validar que la partición existe
    if (mountedPartitions.find(partitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada con ID: " + partitionId;
    }
    
    std::string diskPath = mountedPartitions[partitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición usando partitionInfo
    if (partitionInfo.find(partitionId) == partitionInfo.end()) {
        return "Error: Partición no encontrada.";
    }
    int partIndex = partitionInfo[partitionId].second;
    Partition* part = &mbr.mbr_partitions[partIndex];
    
    // Delegar a ReportGenerator
    ReportGenerator generator;
    
    // Mapear parámetros: si viene -path_file_ls, usar como -ruta para ReportGenerator
    std::map<std::string, std::string> repParams = params;
    if (repParams.count("path_file_ls") && !repParams.count("ruta")) {
        repParams["ruta"] = repParams["path_file_ls"];
    }
    
    return generator.generateReport(repName, repPath, partitionId, diskPath, mbr, part, repParams);
}

// ============ FUNCIONES HELPER PARA FILESYSTEM ============

bool CommandHandler::getInodeFromNum(const std::string& diskPath, int partStart, int inodeNum, Inodo& ino) {
    return DiskManager::readInodo(diskPath, partStart, inodeNum, ino);
}

int CommandHandler::findInodeInDirectory(const std::string& diskPath, int partStart, int parentInode,
                                        const std::string& name, Inodo& parentIno) {
    // Leer el inodo del padre
    if (!DiskManager::readInodo(diskPath, partStart, parentInode, parentIno)) {
        return -1;
    }
    
    // Iterar por todos los bloques del directorio (ahora soporta múltiples bloques)
    for (int i = 0; i < 15 && parentIno.i_block[i] != -1; i++) {
        BlockFolder dirBlock;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], 
                                (char*)&dirBlock, sizeof(BlockFolder))) {
            continue;
        }
        
        // Buscar en todas las 4 entradas del bloque
        for (int j = 0; j < 4; j++) {
            if (dirBlock.b_content[j].b_inodo != 0) {
                std::string entryName(dirBlock.b_content[j].b_name, sizeof(dirBlock.b_content[j].b_name));
                size_t nullPos = entryName.find('\0');
                if (nullPos != std::string::npos) {
                    entryName = entryName.substr(0, nullPos);
                }
                if (entryName == name) {
                return dirBlock.b_content[j].b_inodo;
                }
            }
        }
    }
    
    return -1;
}

int CommandHandler::getInodeFromPath(const std::string& diskPath, int partStart, 
                                    const std::string& path, Inodo& resultIno, char& type) {
    // Parsear la ruta
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

    for (auto& part : parts) {
        if (part.size() > 11) {
            part = part.substr(0, 11);
        }
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return -1;
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
        if (currentInoData.i_type == 1 || currentInoData.i_type == '1') {
            return -1;  // Ruta inválida, no es directorio
        }
        
        // Buscar la parte actual en el directorio
        int nextInode = findInodeInDirectory(diskPath, partStart, currentInode, parts[i], currentInoData);
        if (nextInode < 0) {
            return -1;  // Parte no encontrada
        }
        
        currentInode = nextInode;
        
        if (i == parts.size() - 1) {
            // Última parte - cargar metadata del target real
            if (!DiskManager::readInodo(diskPath, partStart, currentInode, resultIno)) {
                return -1;
            }
            type = resultIno.i_type;
            return currentInode;
        }
    }
    
    return currentInode;
}

bool CommandHandler::createParentDirs(const std::string& diskPath, int partStart, int superblockStart,
                                    const std::string& path) {
    // Parsear la ruta para obtener las carpetas que hay que crear
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
    // Si path termina con /, no hay nada más por agregar
    if (!current.empty()) {
        parts.push_back(current);
    }

    for (auto& part : parts) {
        if (part.size() > 11) {
            part = part.substr(0, 11);
        }
    }
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return false;
    }
    
    // Empezar desde raíz
    int currentInode = 0;
    Inodo currentInoData;
    
    // Crear cada carpeta padre que no existe
    for (size_t i = 0; i < parts.size(); i++) {
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, currentInoData)) {
            return false;
        }
        
        // Buscar si ya existe
        int existingInode = findInodeInDirectory(diskPath, partStart, currentInode, parts[i], currentInoData);
        if (existingInode >= 0) {
            // Ya existe, continuar
            currentInode = existingInode;
            continue;
        }
        
        // Crear nueva carpeta
        int newInode = DiskManager::allocateInode(diskPath, partStart, sb);
        if (newInode < 0) {
            return false;
        }
        
        int dirBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (dirBlock < 0) {
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Crear inodo del directorio
        Inodo newDirIno;
        newDirIno.i_uid = currentUserUid;
        newDirIno.i_gid = currentUserGid;
        newDirIno.i_s = 0;
        newDirIno.i_atime = time(nullptr);
        newDirIno.i_ctime = time(nullptr);
        newDirIno.i_mtime = time(nullptr);
        newDirIno.i_type = 0;  // 0 = carpeta (directorio)
        std::strncpy(newDirIno.i_perm, "664", 3);
        
        for (int j = 0; j < 15; j++) {
            newDirIno.i_block[j] = (j == 0) ? dirBlock : -1;
        }
        
        // Inicializar bloque del directorio vacío
        BlockFolder newDirBlock;
        std::memset(&newDirBlock, 0, sizeof(BlockFolder));

        // Registro 0: . apunta al inodo actual
        std::strncpy(newDirBlock.b_content[0].b_name, ".", 11);
        newDirBlock.b_content[0].b_name[11] = '\0';
        newDirBlock.b_content[0].b_inodo = newInode;

        // Registro 1: .. apunta al padre
        std::strncpy(newDirBlock.b_content[1].b_name, "..", 11);
        newDirBlock.b_content[1].b_name[11] = '\0';
        newDirBlock.b_content[1].b_inodo = currentInode;
        
        // Escribir bloque del directorio
        DiskManager::writeBlock(diskPath, partStart, dirBlock, (char*)&newDirBlock, sizeof(BlockFolder));
        
        if (!DiskManager::writeInodo(diskPath, partStart, newInode, newDirIno)) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Leer el directorio padre
        Inodo parentDirIno;
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, parentDirIno)) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Agregar entrada en el directorio padre, expandiendo si hace falta
        bool added = false;
        int parentStartSlot = (currentInode == 0) ? 0 : 2;
        for (int j = 0; j < 15 && parentDirIno.i_block[j] != -1; j++) {
            BlockFolder folderBlock;
            if (!DiskManager::readBlock(diskPath, partStart, parentDirIno.i_block[j],
                                    (char*)&folderBlock, sizeof(BlockFolder))) {
                continue;
            }
            
            for (int k = parentStartSlot; k < 4; k++) {
                if (folderBlock.b_content[k].b_inodo == 0) {
                    std::strncpy(folderBlock.b_content[k].b_name, parts[i].c_str(), 11);
                    folderBlock.b_content[k].b_name[11] = '\0';
                    folderBlock.b_content[k].b_inodo = newInode;
                    
                    if (!DiskManager::writeBlock(diskPath, partStart, parentDirIno.i_block[j],
                                            (char*)&folderBlock, sizeof(BlockFolder))) {
                        DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                        DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                        return false;
                    }
                    added = true;
                    break;
                }
            }
            if (added) break;
        }

        if (!added) {
            int newBlockForParent = DiskManager::allocateBlock(diskPath, partStart, sb);
            if (newBlockForParent < 0) {
                DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                return false;
            }

            bool linked = false;
            for (int j = 0; j < 15; j++) {
                if (parentDirIno.i_block[j] == -1) {
                    parentDirIno.i_block[j] = newBlockForParent;
                    linked = true;
                    break;
                }
            }

            if (!linked) {
                DiskManager::deallocateBlock(diskPath, partStart, newBlockForParent, sb);
                DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                return false;
            }

            BlockFolder newParentBlock;
            std::memset(&newParentBlock, 0, sizeof(BlockFolder));
            std::strncpy(newParentBlock.b_content[0].b_name, parts[i].c_str(), 11);
            newParentBlock.b_content[0].b_name[11] = '\0';
            newParentBlock.b_content[0].b_inodo = newInode;

            if (!DiskManager::writeBlock(diskPath, partStart, newBlockForParent, (char*)&newParentBlock, sizeof(BlockFolder))) {
                DiskManager::deallocateBlock(diskPath, partStart, newBlockForParent, sb);
                DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                return false;
            }

            if (!DiskManager::writeInodo(diskPath, partStart, currentInode, parentDirIno)) {
                DiskManager::deallocateBlock(diskPath, partStart, newBlockForParent, sb);
                DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                return false;
            }

            added = true;
        }

        if (!added) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        currentInode = newInode;
    }
    
    // Actualizar superblock con los cambios
    if (!DiskManager::writeSuperblock(diskPath, partStart, sb)) {
        return false;
    }
    
    return true;
}

std::string CommandHandler::validateMandatoryParams(const std::map<std::string, std::string>& params,
                                                const std::vector<std::string>& mandatory) {
    for (const auto& param : mandatory) {
        if (!CommandParser::hasParameter(params, param)) {
            return "Error: Parámetro obligatorio faltante: -" + param;
        }
    }
    return "";
}

std::string CommandHandler::validateAllowedParams(const std::map<std::string, std::string>& params,
                                                const std::vector<std::string>& allowed) {
    for (const auto& param : params) {
        bool isAllowed = false;
        for (const auto& allowedParam : allowed) {
            if (param.first == allowedParam) {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed) {
            return "Error: Parámetro desconocido: -" + param.first;
        }
    }
    return "";
}

bool CommandHandler::hasWritePermission(const Inodo& ino, int uid, int gid) {
    // Revisar si el usuario tiene permiso de escritura
    if (uid == ino.i_uid) {
        // Usuario es el dueño, revisar bits de usuario (bit 1 = write)
        return (ino.i_perm[0] & 2) != 0;
    } else if (gid == ino.i_gid) {
        // Usuario está en el grupo, revisar bits de grupo
        return (ino.i_perm[1] & 2) != 0;
    } else {
        // Otros permisos
        return (ino.i_perm[2] & 2) != 0;
    }
}

bool CommandHandler::hasReadPermission(const Inodo& ino, int uid, int gid) {
    // Revisar si el usuario tiene permiso de lectura
    if (uid == ino.i_uid) {
        // Usuario es el dueño, revisar bits de usuario (bit 4 = read)
        return (ino.i_perm[0] & 4) != 0;
    } else if (gid == ino.i_gid) {
        // Usuario está en el grupo, revisar bits de grupo
        return (ino.i_perm[1] & 4) != 0;
    } else {
        // Otros permisos
        return (ino.i_perm[2] & 4) != 0;
    }
}

bool CommandHandler::removeRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                                     int uid, int gid, Superblock& sb) {
    Inodo ino;
    if (!DiskManager::readInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }

    // Verificar permisos de escritura sobre el objeto actual
    if (!hasWritePermission(ino, uid, gid)) {
        return false;
    }

    // Si es directorio, destruir primero su contenido (excepto . y ..)
    if (ino.i_type == 0) {
        std::vector<int> childInodes;

        for (int i = 0; i < 12 && ino.i_block[i] >= 0; i++) {
            BlockFolder blockData;
            if (!DiskManager::readBlock(diskPath, partStart, ino.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
                continue;
            }

            for (int j = 0; j < 4; j++) {
                if (blockData.b_content[j].b_inodo > 0 &&
                    std::string(blockData.b_content[j].b_name) != "." &&
                    std::string(blockData.b_content[j].b_name) != "..") {
                    childInodes.push_back(blockData.b_content[j].b_inodo);
                }
            }
        }

        for (int childInodeNum : childInodes) {
            if (!removeRecursive(diskPath, partStart, childInodeNum, uid, gid, sb)) {
                return false;
            }
        }
    }

    // Liberar bloques y actualizar bitmap de bloques
    for (int i = 0; i < 12 && ino.i_block[i] >= 0; i++) {
        int blockNum = ino.i_block[i];
        if (!DiskManager::deallocateBlock(diskPath, partStart, blockNum, sb)) {
            return false;
        }
        ino.i_block[i] = -1;
    }

    // Limpiar inodo antes de liberarlo en bitmap
    ino.i_s = 0;
    if (!DiskManager::writeInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }

    // Liberar inodo en bitmap (actualiza contador y superblock)
    if (!DiskManager::deallocateInode(diskPath, partStart, inodeNum, sb)) {
        return false;
    }

    return true;
}

std::string CommandHandler::cmdRemove(const std::map<std::string, std::string>& params) {
    // Validar si el usuario está logueado
    if (!isLoggedIn) {
        return "Error: Usuario no autenticado.";
    }
    
    // Validar parámetro obligatorio
    std::vector<std::string> mandatory = {"path"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    
    // Usar la partición de la sesión actual
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: La partición actual no está montada.";
    }

    std::string diskPath = mountedPartitions[currentPartitionId];
    std::string partitionId = currentPartitionId;
    
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR para obtener el offset de inicio
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Normalizar path
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    
    // No permitir borrar la raíz
    if (path == "/") {
        return "Error: No se puede eliminar el directorio raíz.";
    }
    
    // Encontrar la última "/" para obtener el padre
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string targetName = path.substr(lastSlash + 1);
    if (targetName.size() > 11) {
        targetName = targetName.substr(0, 11);
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Navegar al padre
    Inodo parentIno;
    char parentType;
    int parentInodeNum = getInodeFromPath(diskPath, partStart, parentPath, parentIno, parentType);
    
    if (parentInodeNum < 0 || parentType != 0) {
        return "Error: El directorio padre no existe.";
    }
    
    // Verificar permisos de escritura en el directorio padre
    if (!hasWritePermission(parentIno, currentUserUid, currentUserGid)) {
        return "Error: Permiso denegado. No tiene permiso de escritura en el directorio.";
    }
    
    // Buscar el archivo/directorio a eliminar en el padre
    int targetInodeNum = -1;
    
    for (int i = 0; i < 12 && parentIno.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (blockData.b_content[j].b_inodo > 0 && 
                std::string(blockData.b_content[j].b_name) == targetName) {
                targetInodeNum = blockData.b_content[j].b_inodo;
                break;
            }
        }
        
        if (targetInodeNum != -1) break;
    }
    
    if (targetInodeNum < 0) {
        return "Error: El archivo o directorio '" + targetName + "' no existe.";
    }
    
    // Llamar a removeRecursive
    if (!removeRecursive(diskPath, partStart, targetInodeNum, currentUserUid, currentUserGid, sb)) {
        return "Error: No se pudo eliminar por permisos insuficientes o error en eliminación.";
    }
    
    // Actualizar superblock
    DiskManager::writeSuperblock(diskPath, partStart, sb);
    
    // Remover la entrada del directorio padre
    for (int i = 0; i < 12 && parentIno.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (blockData.b_content[j].b_inodo == targetInodeNum) {
                blockData.b_content[j].b_inodo = 0;
                std::memset(blockData.b_content[j].b_name, 0, sizeof(blockData.b_content[j].b_name));
                DiskManager::writeBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder));
                break;
            }
        }
    }
    
    // Decrementar contador de entradas en el padre
    parentIno.i_s--;
    DiskManager::writeInodo(diskPath, partStart, parentInodeNum, parentIno);
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "remove", 9);
        strncpy(info.i_path, targetName.c_str(), 31);
        strncpy(info.i_content, "Archivo eliminado", 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, partStart, info);
    }
    
    return "Éxito: Archivo o directorio eliminado correctamente.";
}

std::string CommandHandler::cmdRename(const std::map<std::string, std::string>& params) {
    // Validar si el usuario está logueado
    if (!isLoggedIn) {
        return "Error: Usuario no autenticado.";
    }
    
    // Validar parámetros obligatorios
    std::vector<std::string> mandatory = {"path", "name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    std::string newName = CommandParser::getParameter(params, "name");
    auto normalizeFsName = [](const std::string& value) {
        return value.size() > 11 ? value.substr(0, 11) : value;
    };
    newName = normalizeFsName(newName);
    
    // Verificar que el nuevo nombre no esté vacío
    if (newName.empty() || newName.length() > 26) {
        return "Error: El nombre del archivo debe tener entre 1 y 26 caracteres.";
    }
    
    // Buscar una partición montada
    if (mountedPartitions.empty()) {
        return "Error: No hay particiones montadas.";
    }
    
    // Usar la primera partición montada
    auto it = mountedPartitions.begin();
    std::string diskPath = it->second;
    std::string partitionId = it->first;
    
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR para obtener el offset de inicio
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Normalizar path
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    
    // No permitir renombrar la raíz
    if (path == "/") {
        return "Error: No se puede renombrar el directorio raíz.";
    }
    
    // Encontrar la última "/" para obtener el padre
    size_t lastSlash = path.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : path.substr(0, lastSlash);
    std::string oldName = normalizeFsName(path.substr(lastSlash + 1));
    
    // Navegar al padre
    Inodo parentIno;
    char parentType;
    int parentInodeNum = getInodeFromPath(diskPath, partStart, parentPath, parentIno, parentType);
    
    if (parentInodeNum < 0 || parentType != 0) {
        return "Error: El directorio padre no existe.";
    }
    
    // Verificar permisos de escritura sobre el archivo/directorio a renombrar
    // Primero encontramos el inodo actual
    Inodo targetIno;
    int targetInodeNum = -1;
    int targetBlockIdx = -1;
    int targetEntryIdx = -1;
    
    for (int i = 0; i < 12 && parentIno.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (blockData.b_content[j].b_inodo != -1) {
                std::string currentName(blockData.b_content[j].b_name, sizeof(blockData.b_content[j].b_name));
                size_t nullPos = currentName.find('\0');
                if (nullPos != std::string::npos) {
                    currentName = currentName.substr(0, nullPos);
                }
                if (currentName == oldName) {
                    targetInodeNum = blockData.b_content[j].b_inodo;
                    targetBlockIdx = i;
                    targetEntryIdx = j;
                    break;
                }
            }
        }
        
        if (targetInodeNum != -1) break;
    }
    
    if (targetInodeNum < 0) {
        return "Error: El archivo o directorio '" + oldName + "' no existe.";
    }
    
    // Leer el inodo del archivo/directorio a renombrar
    if (!DiskManager::readInodo(diskPath, partStart, targetInodeNum, targetIno)) {
        return "Error: No se pudo leer el inodo del archivo.";
    }
    
    // Verificar permiso de escritura sobre el archivo/directorio
    if (!hasWritePermission(targetIno, currentUserUid, currentUserGid)) {
        return "Error: Permiso denegado. No tiene permiso de escritura sobre el archivo o directorio.";
    }
    
    // Verificar que NO exista un archivo con el nuevo nombre en el mismo directorio
    for (int i = 0; i < 12 && parentIno.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (blockData.b_content[j].b_inodo != -1) {
                std::string currentName(blockData.b_content[j].b_name, sizeof(blockData.b_content[j].b_name));
                size_t nullPos = currentName.find('\0');
                if (nullPos != std::string::npos) {
                    currentName = currentName.substr(0, nullPos);
                }
                if (currentName == newName) {
                    return "Error: Ya existe un archivo o directorio con el nombre '" + newName + "' en este directorio.";
                }
            }
        }
    }
    
    // Renombrar: actualizar la entrada en el directorio padre
    BlockFolder blockData;
    if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[targetBlockIdx], 
                               (char*)&blockData, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el bloque del directorio.";
    }
    
    // Cambiar el nombre en la entrada del directorio
    std::memset(blockData.b_content[targetEntryIdx].b_name, 0, 27);
    std::strncpy(blockData.b_content[targetEntryIdx].b_name, newName.c_str(), 11);
    blockData.b_content[targetEntryIdx].b_name[11] = '\0';
    
    // Escribir el bloque actualizado
    if (!DiskManager::writeBlock(diskPath, partStart, parentIno.i_block[targetBlockIdx], 
                                (char*)&blockData, sizeof(BlockFolder))) {
        return "Error: No se pudo escribir el bloque del directorio.";
    }
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock para el journal.";
    }
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "rename", 9);
        strncpy(info.i_path, oldName.c_str(), 31);
        strncpy(info.i_content, newName.c_str(), 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, partStart, info);
    }
    
    return "Éxito: Archivo o directorio renombrado de '" + oldName + "' a '" + newName + "' correctamente.";
}

bool CommandHandler::copyRecursive(const std::string& diskPath, int partStart, int srcInodeNum, int destInodeNum,
                                   const std::string& srcName, int uid, int gid, Superblock& sb) {
    Inodo srcIno;
    if (!DiskManager::readInodo(diskPath, partStart, srcInodeNum, srcIno)) {
        return false;
    }
    
    // Truncar el nombre de forma segura
    std::string safeName = srcName;
    if (safeName.length() > 11) safeName = safeName.substr(0, 11);
    
    if (srcIno.i_type == 1) {
        Inodo newIno;
        // --- FIX 1: LIMPIAR MEMORIA BASURA ---
        for(int i = 0; i < 15; i++) newIno.i_block[i] = -1;
        
        newIno.i_uid = uid;
        newIno.i_gid = gid;
        newIno.i_type = 1;
        newIno.i_s = srcIno.i_s;
        std::strncpy(newIno.i_perm, "644", 3);
        newIno.i_ctime = time(nullptr);
        
        int blockCount = (srcIno.i_s + 63) / 64;
        for (int i = 0; i < blockCount && i < 12; i++) {
            if (srcIno.i_block[i] >= 0) {
                char blockBuffer[64];
                int readSize = std::min(64, srcIno.i_s - (i * 64));
                
                if (!DiskManager::readBlock(diskPath, partStart, srcIno.i_block[i], blockBuffer, readSize)) return false;
                
                int newBlockNum = DiskManager::allocateBlock(diskPath, partStart, sb);
                if (newBlockNum < 0) return false;
                
                if (!DiskManager::writeBlock(diskPath, partStart, newBlockNum, blockBuffer, readSize)) return false;
                
                newIno.i_block[i] = newBlockNum;
            }
        }
        
        int newInodeNum = DiskManager::allocateInode(diskPath, partStart, sb);
        if (newInodeNum < 0) return false;
        
        if (!DiskManager::writeInodo(diskPath, partStart, newInodeNum, newIno)) return false;
        
        Inodo destIno;
        if (!DiskManager::readInodo(diskPath, partStart, destInodeNum, destIno)) return false;
        
        bool added = false;
        for (int i = 0; i < 12; i++) {
            if (destIno.i_block[i] >= 0) {
                BlockFolder blockData;
                if (!DiskManager::readBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) continue;
                
                for (int j = 0; j < 4; j++) {
                    // Validar si el slot es libre (usualmente <= 0 o vacío)
                    if (blockData.b_content[j].b_inodo <= 0) { 
                        blockData.b_content[j].b_inodo = newInodeNum;
                        std::memset(blockData.b_content[j].b_name, 0, 12);
                        std::strncpy(blockData.b_content[j].b_name, safeName.c_str(), 11);
                        
                        DiskManager::writeBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder));
                        added = true; break;
                    }
                }
            } else if (!added) {
                int newDirBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
                destIno.i_block[i] = newDirBlock;
                
                BlockFolder newBlockData;
                std::memset(&newBlockData, 0, sizeof(BlockFolder));
                // Llenar todo con -1 para indicar vacío
                for(int k=0; k<4; k++) newBlockData.b_content[k].b_inodo = -1;
                
                newBlockData.b_content[0].b_inodo = newInodeNum;
                std::strncpy(newBlockData.b_content[0].b_name, safeName.c_str(), 11);
                
                DiskManager::writeBlock(diskPath, partStart, newDirBlock, (char*)&newBlockData, sizeof(BlockFolder));
                added = true; break;
            }
            if (added) break;
        }
        
        if (added) {
            destIno.i_s++;
            DiskManager::writeInodo(diskPath, partStart, destInodeNum, destIno);
            return true;
        }
        return false;
    }
    
    if (srcIno.i_type == 0) {
        Inodo newDirIno;
        // --- FIX 2: LIMPIAR MEMORIA BASURA ---
        for(int i = 0; i < 15; i++) newDirIno.i_block[i] = -1;
        
        newDirIno.i_uid = uid;
        newDirIno.i_gid = gid;
        newDirIno.i_type = 0;
        newDirIno.i_s = 0; 
        std::strncpy(newDirIno.i_perm, "755", 3);
        newDirIno.i_ctime = time(nullptr);
        
        int newBlockNum = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (newBlockNum < 0) return false;
        newDirIno.i_block[0] = newBlockNum;
        
        int newDirInodeNum = DiskManager::allocateInode(diskPath, partStart, sb);
        if (newDirInodeNum < 0) return false;
        
        if (!DiskManager::writeInodo(diskPath, partStart, newDirInodeNum, newDirIno)) return false;
        
        BlockFolder emptyBlock;
        std::memset(&emptyBlock, 0, sizeof(BlockFolder));
        for(int k=0; k<4; k++) emptyBlock.b_content[k].b_inodo = -1; // -1 es vacío
        
        emptyBlock.b_content[0].b_inodo = newDirInodeNum;
        std::strcpy(emptyBlock.b_content[0].b_name, ".");
        emptyBlock.b_content[1].b_inodo = destInodeNum;
        std::strcpy(emptyBlock.b_content[1].b_name, "..");
        DiskManager::writeBlock(diskPath, partStart, newBlockNum, (char*)&emptyBlock, sizeof(BlockFolder));
        
        Inodo destIno;
        if (!DiskManager::readInodo(diskPath, partStart, destInodeNum, destIno)) return false;
        
        bool added = false;
        for (int i = 0; i < 12; i++) {
            if (destIno.i_block[i] >= 0) {
                BlockFolder blockData;
                if (!DiskManager::readBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) continue;
                
                for (int j = 0; j < 4; j++) {
                    if (blockData.b_content[j].b_inodo <= 0) {
                        blockData.b_content[j].b_inodo = newDirInodeNum;
                        std::memset(blockData.b_content[j].b_name, 0, 12);
                        std::strncpy(blockData.b_content[j].b_name, safeName.c_str(), 11);
                        
                        DiskManager::writeBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder));
                        added = true; break;
                    }
                }
            } else if (!added) {
                int parentNewBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
                destIno.i_block[i] = parentNewBlock;
                
                BlockFolder parentBlockData;
                std::memset(&parentBlockData, 0, sizeof(BlockFolder));
                for(int k=0; k<4; k++) parentBlockData.b_content[k].b_inodo = -1;
                
                parentBlockData.b_content[0].b_inodo = newDirInodeNum;
                std::strncpy(parentBlockData.b_content[0].b_name, safeName.c_str(), 11);
                
                DiskManager::writeBlock(diskPath, partStart, parentNewBlock, (char*)&parentBlockData, sizeof(BlockFolder));
                added = true; break;
            }
            if (added) break;
        }
        
        if (!added) return false;
        
        destIno.i_s++;
        DiskManager::writeInodo(diskPath, partStart, destInodeNum, destIno);
        
        for (int i = 0; i < 12 && srcIno.i_block[i] >= 0; i++) {
            BlockFolder blockData;
            if (!DiskManager::readBlock(diskPath, partStart, srcIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) continue;
            
            for (int j = 0; j < 4; j++) {
                if (blockData.b_content[j].b_inodo > 0) { // Mayor a 0 para ignorar -1 y la raiz(0)
                    
                    // --- FIX 3: LECTURA SEGURA DEL NOMBRE ---
                    char safeChildName[13] = {0};
                    std::strncpy(safeChildName, blockData.b_content[j].b_name, 12);
                    std::string childNameStr(safeChildName);

                    if (childNameStr != "." && childNameStr != "..") {
                        Inodo childIno;
                        if (!DiskManager::readInodo(diskPath, partStart, blockData.b_content[j].b_inodo, childIno)) continue;
                        if (!hasReadPermission(childIno, uid, gid)) continue;
                        
                        copyRecursive(diskPath, partStart, blockData.b_content[j].b_inodo, newDirInodeNum,
                                      childNameStr, uid, gid, sb);
                    }
                }
            }
        }
        return true;
    }
    return false;
}

std::string CommandHandler::cmdCopy(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) return "Error: Usuario no autenticado.";
    
    std::vector<std::string> mandatory = {"path", "destino"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string srcPath = CommandParser::getParameter(params, "path");
    std::string destPath = CommandParser::getParameter(params, "destino");
    
    // CORRECCIÓN: Usar la partición activa de la sesión
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: La partición de la sesión actual no está montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    auto partInfo = partitionInfo[currentPartitionId];
    int partIndex = partInfo.second;
    
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) return "Error: No se pudo leer el MBR.";
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    if (srcPath.empty() || srcPath[0] != '/') srcPath = "/" + srcPath;
    if (destPath.empty() || destPath[0] != '/') destPath = "/" + destPath;
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) return "Error: No se pudo leer el superblock.";
    
    Inodo srcIno;
    char srcType;
    int srcInodeNum = getInodeFromPath(diskPath, partStart, srcPath, srcIno, srcType);
    if (srcInodeNum < 0) return "Error: La ruta de origen '" + srcPath + "' no existe.";
    if (!hasReadPermission(srcIno, currentUserUid, currentUserGid)) return "Error: Permiso de lectura denegado en origen.";
    
    Inodo destIno;
    char destType;
    int destInodeNum = getInodeFromPath(diskPath, partStart, destPath, destIno, destType);
    if (destInodeNum < 0 || destType != 0) return "Error: El directorio de destino '" + destPath + "' no existe o no es carpeta.";
    if (!hasWritePermission(destIno, currentUserUid, currentUserGid)) return "Error: Permiso de escritura denegado en destino.";
    
    size_t lastSlash = srcPath.find_last_of('/');
    std::string srcName = srcPath.substr(lastSlash + 1);
    
    if (!copyRecursive(diskPath, partStart, srcInodeNum, destInodeNum, srcName, currentUserUid, currentUserGid, sb)) {
        return "Error: No se pudo completar la copia.";
    }
    
    DiskManager::writeSuperblock(diskPath, partStart, sb);
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "copy", 9);
        strncpy(info.i_path, srcPath.c_str(), 31);
        strncpy(info.i_content, destPath.c_str(), 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, partStart, info);
    }
    
    return "Éxito: Copia completada de '" + srcPath + "' a '" + destPath + "' correctamente.";
}

std::string CommandHandler::cmdMove(const std::map<std::string, std::string>& params) {

    if (!isLoggedIn) return "Error: Usuario no autenticado.";
    
    std::vector<std::string> mandatory = {"path", "destino"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string srcPath = CommandParser::getParameter(params, "path");
    std::string destPath = CommandParser::getParameter(params, "destino");
    
    //  Usar la partición activa
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: La partición de la sesión actual no está montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    auto partInfo = partitionInfo[currentPartitionId];
    int partIndex = partInfo.second;
    
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) return "Error: No se pudo leer el MBR.";
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    if (srcPath.empty() || srcPath[0] != '/') srcPath = "/" + srcPath;
    if (destPath.empty() || destPath[0] != '/') destPath = "/" + destPath;
    if (srcPath == "/") return "Error: No se puede mover el directorio raíz.";
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) return "Error: No se pudo leer el superblock.";
    
    Inodo srcIno; char srcType;
    int srcInodeNum = getInodeFromPath(diskPath, partStart, srcPath, srcIno, srcType);
    if (srcInodeNum < 0) return "Error: La ruta de origen '" + srcPath + "' no existe.";
    if (!hasWritePermission(srcIno, currentUserUid, currentUserGid)) return "Error: Permiso denegado en origen.";
    
    Inodo destIno; char destType;
    int destInodeNum = getInodeFromPath(diskPath, partStart, destPath, destIno, destType);
    if (destInodeNum < 0 || destType != 0) return "Error: El destino '" + destPath + "' no existe o no es carpeta.";
    if (!hasWritePermission(destIno, currentUserUid, currentUserGid)) return "Error: Permiso denegado en destino.";
    
    size_t lastSlash = srcPath.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : srcPath.substr(0, lastSlash);
    std::string fileName = srcPath.substr(lastSlash + 1);
    
    // Truncar el nombre a 11 para poder buscarlo y guardarlo
    std::string safeName = fileName;
    if (safeName.length() > 11) safeName = safeName.substr(0, 11);
    
    Inodo parentIno; char parentType;
    int parentInodeNum = getInodeFromPath(diskPath, partStart, parentPath, parentIno, parentType);
    if (parentInodeNum < 0 || parentType != 0) return "Error: El directorio padre no existe.";
    
    bool removedFromParent = false;
    for (int i = 0; i < 12 && parentIno.i_block[i] >= 0 && !removedFromParent; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) continue;
        
        for (int j = 0; j < 4; j++) {
            // Validar contra el nombre truncado (safeName)
            if (blockData.b_content[j].b_inodo == srcInodeNum && 
                std::string(blockData.b_content[j].b_name) == safeName) {
                
                // Usar 0 para liberar y memset de 12
                blockData.b_content[j].b_inodo = 0;
                std::memset(blockData.b_content[j].b_name, 0, 12);
                
                if (DiskManager::writeBlock(diskPath, partStart, parentIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
                    removedFromParent = true;
                    parentIno.i_s--;
                }
                break;
            }
        }
    }
    
    if (!removedFromParent) return "Error: No se pudo remover el archivo del directorio origen.";
    DiskManager::writeInodo(diskPath, partStart, parentInodeNum, parentIno);
    
    bool addedToDest = false;
    for (int i = 0; i < 12; i++) {
        if (destIno.i_block[i] >= 0) {
            BlockFolder blockData;
            if (!DiskManager::readBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) continue;
            
            for (int j = 0; j < 4; j++) {
                if (blockData.b_content[j].b_inodo == 0) { // Encontramos espacio
                    blockData.b_content[j].b_inodo = srcInodeNum;
                    std::memset(blockData.b_content[j].b_name, 0, 12);
                    std::strncpy(blockData.b_content[j].b_name, safeName.c_str(), 11);
                    
                    if (DiskManager::writeBlock(diskPath, partStart, destIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
                        addedToDest = true;
                        destIno.i_s++;
                    }
                    break;
                }
            }
        } else if (!addedToDest) {
            // CORRECCIÓN: ¡El destino está lleno! Pedimos un bloque nuevo para expandirlo
            int newBlockNum = DiskManager::allocateBlock(diskPath, partStart, sb);
            if (newBlockNum < 0) return "Error: No hay bloques disponibles para expandir el destino.";
            
            destIno.i_block[i] = newBlockNum;
            BlockFolder newBlockData;
            std::memset(&newBlockData, 0, sizeof(BlockFolder));
            newBlockData.b_content[0].b_inodo = srcInodeNum;
            std::strncpy(newBlockData.b_content[0].b_name, safeName.c_str(), 11);
            
            if (DiskManager::writeBlock(diskPath, partStart, newBlockNum, (char*)&newBlockData, sizeof(BlockFolder))) {
                addedToDest = true;
                destIno.i_s++;
            }
            break;
        }
        if (addedToDest) break;
    }
    
    if (!addedToDest) return "Error: No se pudo agregar el archivo al directorio destino.";
    
    DiskManager::writeInodo(diskPath, partStart, destInodeNum, destIno);
    DiskManager::writeSuperblock(diskPath, partStart, sb);
    
    if(sb.s_filesystem_type == 3){
        Information info;
        memset(&info, 0, sizeof(Information));
        strncpy(info.i_operation, "move", 9);
        strncpy(info.i_path, srcPath.c_str(), 31);
        strncpy(info.i_content, destPath.c_str(), 63);
        info.i_date = (float)time(nullptr);
        DiskManager::addJournalEntry(diskPath, partStart, info);
    }
    
    return "Éxito: Movimiento completado de '" + srcPath + "' a '" + destPath + "' correctamente.";
}

bool CommandHandler::matchPattern(const std::string& name, const std::string& pattern) {
    int nIdx = 0, pIdx = 0;
    int nLen = name.length(), pLen = pattern.length();
    int starIdx = -1, matchIdx = 0;
    
    while (nIdx < nLen) {
        if (pIdx < pLen && pattern[pIdx] == '?') {
            // ? coincide con exactamente 1 carácter
            nIdx++;
            pIdx++;
        } else if (pIdx < pLen && pattern[pIdx] == '*') {
            // * coincide con 1 o más caracteres
            starIdx = pIdx;
            matchIdx = nIdx;
            pIdx++;
        } else if (pIdx < pLen && pattern[pIdx] == name[nIdx]) {
            // Caracteres coinciden
            nIdx++;
            pIdx++;
        } else if (starIdx != -1) {
            // Backtrack con el último *
            pIdx = starIdx + 1;
            matchIdx++;
            nIdx = matchIdx;
        } else {
            // No coincide
            return false;
        }
    }
    
    // Consumir * restantes al final del patrón
    while (pIdx < pLen && pattern[pIdx] == '*') {
        pIdx++;
    }
    
    return pIdx == pLen;
}

void CommandHandler::findRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                                  const std::string& pattern, int uid, int gid,
                                  std::vector<std::pair<std::string, int>>& results) {
    Inodo ino;
    if (!DiskManager::readInodo(diskPath, partStart, inodeNum, ino)) {
        return;
    }
    
    // Solo procesar directorios
    if (ino.i_type != 0) {
        return;
    }
    
    // Recorrer entradas del directorio
    for (int i = 0; i < 12 && ino.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, ino.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (blockData.b_content[j].b_inodo != -1 && 
                blockData.b_content[j].b_inodo != 0 &&
                blockData.b_content[j].b_name[0] != '\0') {
                std::string className(blockData.b_content[j].b_name, sizeof(blockData.b_content[j].b_name));
                size_t nullPos = className.find('\0');
                if (nullPos != std::string::npos) {
                    className = className.substr(0, nullPos);
                }
                if (className == "." || className == "..") {
                    continue;
                }
                
                // Leer inodo del child
                Inodo childIno;
                if (!DiskManager::readInodo(diskPath, partStart, blockData.b_content[j].b_inodo, childIno)) {
                    continue;
                }
                
                // Verificar permiso de lectura
                if (!hasReadPermission(childIno, uid, gid)) {
                    continue;
                }
                
                // Si coincide el patrón, agregar a resultados
                if (matchPattern(className, pattern)) {
                    results.push_back(std::make_pair(className, blockData.b_content[j].b_inodo));
                }
                
                // Si es un directorio, buscar recursivamente
                if (childIno.i_type == 0) {
                    findRecursive(diskPath, partStart, blockData.b_content[j].b_inodo, pattern, uid, gid, results);
                }
            }
        }
    }
}

std::string CommandHandler::cmdFind(const std::map<std::string, std::string>& params) {
    // Validar si el usuario está logueado
    if (!isLoggedIn) {
        return "Error: Usuario no autenticado.";
    }
    
    // Validar parámetros obligatorios
    std::vector<std::string> mandatory = {"path", "name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string searchPath = CommandParser::getParameter(params, "path");
    std::string pattern = CommandParser::getParameter(params, "name");

    searchPath.erase(std::remove(searchPath.begin(), searchPath.end(), '\"'), searchPath.end());
    searchPath.erase(std::remove(searchPath.begin(), searchPath.end(), '\\'), searchPath.end());
    
    pattern.erase(std::remove(pattern.begin(), pattern.end(), '\"'), pattern.end());
    pattern.erase(std::remove(pattern.begin(), pattern.end(), '\\'), pattern.end());

    auto normalizeFsName = [](const std::string& value) {
        return value.size() > 11 ? value.substr(0, 11) : value;
    };

    bool hasWildcards = pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
    if (!hasWildcards) {
        pattern = normalizeFsName(pattern);
    }
    
    // Buscar una partición montada
    if (mountedPartitions.empty()) {
        return "Error: No hay particiones montadas.";
    }
    
    // Usar la primera partición montada
    auto it = mountedPartitions.begin();
    std::string diskPath = it->second;
    std::string partitionId = it->first;
    
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR para obtener el offset de inicio
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Normalizar path
    if (searchPath.empty() || searchPath[0] != '/') {
        searchPath = "/" + searchPath;
    }
    
    // Navegar a la carpeta de búsqueda
    Inodo searchIno;
    char searchType;
    int searchInodeNum = getInodeFromPath(diskPath, partStart, searchPath, searchIno, searchType);
    
    if (searchInodeNum < 0 || searchType != 0) {
        return "Error: La ruta de búsqueda '" + searchPath + "' no existe o no es un directorio.";
    }
    
    // Recolectar resultados
    std::vector<std::pair<std::string, int>> results;
    findRecursive(diskPath, partStart, searchInodeNum, pattern, currentUserUid, currentUserGid, results);
    
    if (results.empty()) {
        return "No se encontraron archivos o carpetas que coincidan con el patrón '" + pattern + "'.";
    }
    
    // Construir respuesta con árbol de resultados
    std::string response = "Resultados encontrados:\n";
    response += "/ \n";
    
    // Agrupar resultados por directorio padre
    std::map<std::string, std::vector<std::string>> resultsByParent;
    
    for (const auto& result : results) {
        // Para simplificar, mostrar cada resultado con su ruta completa
        response += "  |_ " + result.first + "\n";
    }
    
    return response;
}

bool CommandHandler::chownRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                                   int newUid, int currentUid, bool isRoot) {
    Inodo ino;
    if (!DiskManager::readInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }
    
    // Verificar permisos: solo root o el propietario puede hacer chown
    if (!isRoot && ino.i_uid != currentUid) {
        return false;
    }
    
    // Cambiar propietario
    ino.i_uid = newUid;
    if (!DiskManager::writeInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }
    
    // Si es un directorio, aplicar recursivamente
    if (ino.i_type == 0) {
        for (int i = 0; i < 12 && ino.i_block[i] >= 0; i++) {
            BlockFolder blockData;
            if (!DiskManager::readBlock(diskPath, partStart, ino.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
                continue;
            }
            
            for (int j = 0; j < 4; j++) {
                if (blockData.b_content[j].b_inodo != -1 && 
                    blockData.b_content[j].b_inodo != 0 &&
                    std::string(blockData.b_content[j].b_name) != "." &&
                    std::string(blockData.b_content[j].b_name) != "..") {
                    
                    // Aplicar recursivamente
                    chownRecursive(diskPath, partStart, blockData.b_content[j].b_inodo, newUid, currentUid, isRoot);
                }
            }
        }
    }
    
    return true;
}

std::string CommandHandler::cmdChown(const std::map<std::string, std::string>& params) {
    // Validar si el usuario está logueado
    if (!isLoggedIn) {
        return "Error: Usuario no autenticado.";
    }
    
    // Validar parámetros obligatorios
    std::vector<std::string> mandatory = {"path", "usuario"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    std::string newUserName = CommandParser::getParameter(params, "usuario");
    bool recursive = CommandParser::hasParameter(params, "r");
    
    // Buscar una partición montada
    if (mountedPartitions.empty()) {
        return "Error: No hay particiones montadas.";
    }
    
    // Usar la primera partición montada
    auto it = mountedPartitions.begin();
    std::string diskPath = it->second;
    std::string partitionId = it->first;
    
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR para obtener el offset de inicio
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Normalizar path
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Navegar al archivo/carpeta
    Inodo targetIno;
    char targetType;
    int targetInodeNum = getInodeFromPath(diskPath, partStart, path, targetIno, targetType);
    
    if (targetInodeNum < 0) {
        return "Error: La ruta '" + path + "' no existe.";
    }
    
    // Verificar permisos: solo root o el propietario puede hacer chown
    bool isRoot = (currentUserUid == 0);
    if (!isRoot && targetIno.i_uid != currentUserUid) {
        return "Error: Permiso denegado. Solo el propietario o root puede cambiar el propietario.";
    }
    
    // Obtener el UID del nuevo usuario
    // Para esto, buscamos en el archivo users.txt
    int newUid = -1;
    
    // Obtener raíz
    Inodo rootIno;
    if (!DiskManager::readInodo(diskPath, partStart, 0, rootIno)) {
        return "Error: No se pudo leer la raíz.";
    }
    
    // Buscar users.txt en raíz
    int usersInodeNum = -1;
    for (int i = 0; i < 12 && rootIno.i_block[i] >= 0; i++) {
        BlockFolder blockData;
        if (!DiskManager::readBlock(diskPath, partStart, rootIno.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (std::string(blockData.b_content[j].b_name) == "users.txt") {
                usersInodeNum = blockData.b_content[j].b_inodo;
                break;
            }
        }
        
        if (usersInodeNum != -1) break;
    }
    
    if (usersInodeNum < 0) {
        return "Error: No se pudo encontrar el archivo de usuarios.";
    }
    
    // Leer users.txt y buscar el usuario
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, partStart, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el archivo de usuarios.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile blockData;
        int readSize = std::min(64, usersIno.i_s - (i * 64));
        
        if (!DiskManager::readBlock(diskPath, partStart, usersIno.i_block[i], 
                                (char*)&blockData, readSize)) {
            continue;
        }
        
        usersContent += std::string(blockData.b_content, readSize);
    }
    
    // Parsear users.txt para buscar el UID del nuevo usuario
    // Formato esperado: usuario:uid o similar
    std::istringstream iss(usersContent);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Parsear línea: "usuario,uid,gid"
        std::istringstream lineStream(line);
        std::string userNamePart, uidStr, gidStr;
        
        if (std::getline(lineStream, userNamePart, ',') && 
            std::getline(lineStream, uidStr, ',')) {
            
            if (userNamePart == newUserName) {
                newUid = std::stoi(uidStr);
                break;
            }
        }
    }
    
    if (newUid < 0) {
        return "Error: El usuario '" + newUserName + "' no existe.";
    }
    
    // Aplicar chown
    if (recursive) {
        // Cambiar recursivamente
        if (!chownRecursive(diskPath, partStart, targetInodeNum, newUid, currentUserUid, isRoot)) {
            return "Error: No se pudo completar el cambio de propietario.";
        }
    } else {
        // Solo cambiar el archivo/carpeta especificado
        if (!chownRecursive(diskPath, partStart, targetInodeNum, newUid, currentUserUid, isRoot)) {
            return "Error: No se pudo completar el cambio de propietario.";
        }
    }
    
    // Actualizar superblock
    DiskManager::writeSuperblock(diskPath, partStart, sb);
    
    return "Éxito: Propietario cambiado a '" + newUserName + "' para '" + path + (recursive ? "' recursivamente." : "'.");
}

bool CommandHandler::chmodRecursive(const std::string& diskPath, int partStart, int inodeNum, 
                                   const std::string& perms, int currentUid, bool recursive) {
    Inodo ino;
    if (!DiskManager::readInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }
    
    // Solo cambiar permisos de archivos que pertenecen al usuario actual
    if (ino.i_uid != currentUid) {
        return false;
    }
    
    // Cambiar permisos
    std::strncpy(ino.i_perm, perms.c_str(), 3);
    if (!DiskManager::writeInodo(diskPath, partStart, inodeNum, ino)) {
        return false;
    }
    
    // Si es un directorio, aplicar recursivamente
    if (ino.i_type == 0 && recursive) {
        for (int i = 0; i < 12 && ino.i_block[i] >= 0; i++) {
            BlockFolder blockData;
            if (!DiskManager::readBlock(diskPath, partStart, ino.i_block[i], (char*)&blockData, sizeof(BlockFolder))) {
                continue;
            }
            
            for (int j = 0; j < 4; j++) {
                if (blockData.b_content[j].b_inodo != -1 && 
                    blockData.b_content[j].b_inodo != 0 &&
                    std::string(blockData.b_content[j].b_name) != "." &&
                    std::string(blockData.b_content[j].b_name) != "..") {
                    
                    // Aplicar recursivamente solo si pertenece al usuario actual
                    Inodo childIno;
                    if (!DiskManager::readInodo(diskPath, partStart, blockData.b_content[j].b_inodo, childIno)) {
                        continue;
                    }
                    
                    if (childIno.i_uid == currentUid) {
                        chmodRecursive(diskPath, partStart, blockData.b_content[j].b_inodo, perms, currentUid, recursive);
                    }
                }
            }
        }
    }
    
    return true;
}

std::string CommandHandler::cmdChmod(const std::map<std::string, std::string>& params) {
    // Solo root puede ejecutar chmod
    if (currentUser != "root") {
        return "Error: Solo el usuario root puede utilizar chmod.";
    }
    
    // Validar si el usuario está logueado
    if (!isLoggedIn) {
        return "Error: Usuario no autenticado.";
    }
    
    // Validar parámetros obligatorios
    std::vector<std::string> mandatory = {"path", "ugo"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    std::string ugoStr = CommandParser::getParameter(params, "ugo");
    bool recursive = CommandParser::hasParameter(params, "r");
    
    // Validar formato de -ugo: debe ser exactamente 3 dígitos
    if (ugoStr.length() != 3) {
        return "Error: El formato de -ugo debe ser exactamente 3 dígitos (u,g,o), ej: 755.";
    }
    
    // Validar que cada dígito esté en rango [0-7]
    for (int i = 0; i < 3; i++) {
        if (ugoStr[i] < '0' || ugoStr[i] > '7') {
            return "Error: Cada dígito de -ugo debe estar en rango [0-7], ej: 755.";
        }
    }
    
    // Buscar la particion correcta
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: La partición de la sesión actual no está montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    auto partInfo = partitionInfo[currentPartitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR para obtener el offset de inicio
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Normalizar path
    if (path.empty() || path[0] != '/') {
        path = "/" + path;
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Navegar al archivo/carpeta
    Inodo targetIno;
    char targetType;
    int targetInodeNum = getInodeFromPath(diskPath, partStart, path, targetIno, targetType);
    
    if (targetInodeNum < 0) {
        return "Error: La ruta '" + path + "' no existe.";
    }
    
    // Si no es recursivo, verificar que pertenezca al usuario actual
    if (!recursive && targetIno.i_uid != currentUserUid) {
        return "Error: La carpeta o archivo no pertenece al usuario actual.";
    }
    
    // Aplicar chmod
    if (!chmodRecursive(diskPath, partStart, targetInodeNum, ugoStr, currentUserUid, recursive)) {
        return "Error: No se pudo completar el cambio de permisos.";
    }
    
    // Actualizar superblock
    DiskManager::writeSuperblock(diskPath, partStart, sb);
    
    return "Éxito: Permisos cambiados a " + ugoStr + " para '" + path + (recursive ? "' recursivamente." : "'.");
}

std::string CommandHandler::cmdLoss(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string partitionId = CommandParser::getParameter(params, "id");
    std::transform(partitionId.begin(), partitionId.end(), partitionId.begin(), ::toupper);
    
    if (mountedPartitions.find(partitionId) == mountedPartitions.end()) {
        return "Error: No existe una partición montada con el ID '" + partitionId + "'.";
    }
    
    std::string diskPath = mountedPartitions[partitionId];
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    int partSize = mbr.mbr_partitions[partIndex].part_s; // <-- Aquí tenemos el tamaño total
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    if (sb.s_filesystem_type != 3) {
        return "Error: El comando loss solo funciona con particiones EXT3.";
    }

    std::fstream file(diskPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return "Error: No se pudo abrir el disco para ejecutar loss.";
    }

    file.seekp(partStart); // Nos paramos exactamente donde inicia la partición
    
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer)); // Buffer gigante lleno de ceros (\0)
    
    int written = 0;
    while (written < partSize) {
        int toWrite = std::min((int)sizeof(buffer), partSize - written);
        file.write(buffer, toWrite);
        written += toWrite;
    }
    
    file.close();

    return "Éxito: Pérdida del sistema de archivos EXT3 simulada para la partición '" + partitionId + "'. Todos los datos han sido eliminados.";
}

std::string CommandHandler::cmdJournaling(const std::map<std::string, std::string>& params) {
    // Validar parámetro obligatorio
    std::vector<std::string> mandatory = {"id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string partitionId = CommandParser::getParameter(params, "id");
    std::transform(partitionId.begin(), partitionId.end(), partitionId.begin(), ::toupper);
    
    // Verificar si la partición está montada
    if (mountedPartitions.find(partitionId) == mountedPartitions.end()) {
        return "Error: No existe una partición montada con el ID '" + partitionId + "'.";
    }
    
    std::string diskPath = mountedPartitions[partitionId];
    auto partInfo = partitionInfo[partitionId];
    int partIndex = partInfo.second;
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    int partStart = mbr.mbr_partitions[partIndex].part_start;
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // El journal solo existe en EXT3
    if (sb.s_filesystem_type != 3) {
        return "Error: El comando journaling solo funciona con particiones EXT3.";
    }
    
    // Leer el journal
    Journal journal;
    int journalStart = partStart + sizeof(Superblock);
    
    std::ifstream diskFile(diskPath, std::ios::binary);
    if (!diskFile.is_open()) {
        return "Error: No se pudo abrir el disco.";
    }
    
    diskFile.seekg(journalStart);
    diskFile.read((char*)&journal, sizeof(Journal));
    diskFile.close();
    
    // Construir la tabla con el formato deseado
    std::string result = "Operacion|Path|Contenido|Fecha\n";
    
    // Agregar líneas para cada transacción registrada
    for (int i = 0; i < journal.j_count && i < 50; i++) {
        Information& info = journal.j_content[i];
        
        // Validar que la entrada tenga contenido
        if (std::strlen(info.i_operation) == 0) {
            continue;
        }
        
        // Formatear la fecha
        time_t timestamp = (time_t)info.i_date;
        struct tm* timeinfo = std::localtime(&timestamp);
        char dateStr[20];
        std::strftime(dateStr, sizeof(dateStr), "%d/%m/%Y %H:%M", timeinfo);
        
        // Construir la fila
        result += std::string(info.i_operation) + "|" +
                  std::string(info.i_path) + "|" +
                  std::string(info.i_content) + "|" +
                  std::string(dateStr) + "\n";
    }
    
    // Si no hay entradas, retornar un mensaje
    if (journal.j_count == 0) {
        result = "Éxito: No hay transacciones registradas en el journal de la partición '" + partitionId + "'.";
    }
    
    return result;
}

int CommandHandler::getInodeFromPathPublic(const std::string& diskPath, int partStart, const std::string& path, Inodo& resultIno, char& type) {
    std::vector<std::string> parts;
    std::string current = "";
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) { parts.push_back(current); current = ""; }
        } else { current += c; }
    }
    if (!current.empty()) parts.push_back(current);

    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) return -1;
    
    if (parts.empty()) {
        if (!DiskManager::readInodo(diskPath, partStart, 0, resultIno)) return -1;
        type = resultIno.i_type;
        return 0; // El Inodo raíz SIEMPRE es el 0
    }
    // ----------------------------------------

    int currentInode = 0;
    Inodo currentInoData;
    
    for (size_t i = 0; i < parts.size(); i++) {
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, currentInoData)) return -1;
        if (currentInoData.i_type == '1' || currentInoData.i_type == 1) return -1;
        
        int nextInode = findInodeInDirectory(diskPath, partStart, currentInode, parts[i], currentInoData);
        if (nextInode < 0) return -1;
        currentInode = nextInode;
        
        if (i == parts.size() - 1) {
            if (!DiskManager::readInodo(diskPath, partStart, currentInode, resultIno)) return -1;
            type = resultIno.i_type;
            return currentInode;
        }
    }
    return currentInode;
}

std::string CommandHandler::getDirectoryJSON(const std::string& diskPath, const std::string& partName, const std::string& path) {
    std::cout << "\n=== DEBUG EXPLORE: Disco: " << diskPath << " | Part: " << partName << " | Path: " << path << " ===" << std::endl;
    
    FILE* file = fopen(diskPath.c_str(), "rb");
    if (!file) {
        std::cout << "DEBUG: Error abriendo el disco." << std::endl;
        return "[]";
    }
    
    MBR mbr;
    fread(&mbr, sizeof(MBR), 1, file);
    fclose(file);

    int partStart = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_s > 0) {
            // Limpieza segura del nombre de la partición (IGUAL QUE ANTES)
            char safeName[17] = {0};
            std::strncpy(safeName, mbr.mbr_partitions[i].part_name, 16);
            std::string pName(safeName);
            
            if (pName == partName) {
                partStart = mbr.mbr_partitions[i].part_start;
                std::cout << "DEBUG: Particion encontrada. Inicio en bloque: " << partStart << std::endl;
                break;
            }
        }
    }

    if (partStart == -1) {
        std::cout << "DEBUG: ERROR. No se encontro la particion que coincida con '" << partName << "'" << std::endl;
        return "[]";
    }

    Inodo folderIno;
    char type;
    // Llamamos a tu clon público
    int targetInodeIdx = getInodeFromPathPublic(diskPath, partStart, path, folderIno, type);

    if (targetInodeIdx < 0) {
        std::cout << "DEBUG: ERROR. getInodeFromPathPublic retorno -1. La ruta o Inodo raiz no se encontro." << std::endl;
        return "[]";
    }

    std::cout << "DEBUG: Inodo objetivo encontrado -> Index: " << targetInodeIdx << " | Tipo: " << type << std::endl;

    if (type != '0' && type != 0) {
        std::cout << "DEBUG: ERROR. El inodo encontrado no es una carpeta. (Tipo es: " << type << ")" << std::endl;
        return "[]";
    }

    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return "[]";
    }

    std::string json = "[";
    bool first = true;
    file = fopen(diskPath.c_str(), "rb");
    if (!file) return "[]";

    for (int i = 0; i < 12; i++) { 
        if (folderIno.i_block[i] != -1) {
            BlockFolder bf;
            fseek(file, sb.s_block_start + (folderIno.i_block[i] * sizeof(BlockFolder)), SEEK_SET);
            fread(&bf, sizeof(BlockFolder), 1, file);
            
            for (int j = 0; j < 4; j++) {
                // Verificamos inodo válido y que el nombre no esté en blanco
                if (bf.b_content[j].b_inodo != -1 && bf.b_content[j].b_name[0] != '\0') {
                    
                    // Limpieza segura del nombre del archivo/carpeta
                    char safeEntryName[13] = {0};
                    std::strncpy(safeEntryName, bf.b_content[j].b_name, 12);
                    std::string entryName(safeEntryName);
                    
                    if (entryName == "." || entryName == "..") continue;
                    
                    Inodo childIno;
                    if (DiskManager::readInodo(diskPath, partStart, bf.b_content[j].b_inodo, childIno)) {
                        if (!first) json += ",";
                        
                        // Determinamos el tipo
                        std::string itemType = (childIno.i_type == '0' || childIno.i_type == 0) ? "folder" : "file";
                                                
                        json += "{\"name\":\"" + entryName + "\",\"type\":\"" + itemType + "\"}";
                        first = false;
                    }
                }
            }
        }
    }

    json += "]";
    fclose(file);
    
    return json;
}

std::string CommandHandler::getFileContentJSON(const std::string& diskPath, const std::string& partName, const std::string& path) {
    std::cout << "\n=== DEBUG FILE: Buscando archivo en -> " << path << " ===" << std::endl;
    FILE* file = fopen(diskPath.c_str(), "rb");
    if (!file) return "{\"content\":\"Error: Disco no encontrado\"}";

    MBR mbr;
    fread(&mbr, sizeof(MBR), 1, file);
    fclose(file);

    int partStart = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_s > 0) {
            char safeName[17] = {0};
            std::strncpy(safeName, mbr.mbr_partitions[i].part_name, 16);
            if (std::string(safeName) == partName) {
                partStart = mbr.mbr_partitions[i].part_start;
                break;
            }
        }
    }

    if (partStart == -1) return "{\"content\":\"Error: Particion no encontrada\"}";

    Inodo fileIno;
    char type;
    int targetInodeIdx = getInodeFromPathPublic(diskPath, partStart, path, fileIno, type);

    std::cout << "DEBUG FILE: getInodeFromPathPublic devolvio Index: " << targetInodeIdx << " | Tipo: " << type << " (" << (int)type << ")" << std::endl;

    if (targetInodeIdx < 0) {
        std::cout << "DEBUG FILE: Falla. No se encontro el inodo en la ruta." << std::endl;
        return "{\"content\":\"Error: Archivo no encontrado en el sistema\"}";
    }
    
    if (type == '0' || type == 0) {
        std::cout << "DEBUG FILE: Falla. El inodo pertenece a una carpeta." << std::endl;
        return "{\"content\":\"Error: La ruta apunta a una carpeta, no a un archivo\"}";
    }

    std::cout << "DEBUG FILE: Archivo valido encontrado. Extrayendo contenido..." << std::endl;

    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) return "{\"content\":\"Error leyendo SB\"}";

    std::string textContent = "";
    file = fopen(diskPath.c_str(), "rb");
    if (file) {
        for (int i = 0; i < 12; i++) {
            if (fileIno.i_block[i] != -1) {
                BlockFile bf;
                fseek(file, sb.s_block_start + (fileIno.i_block[i] * sizeof(BlockFile)), SEEK_SET);
                fread(&bf, sizeof(BlockFile), 1, file);
                
                for (int j = 0; j < 64; j++) {
                    // Cuidado aqui: A veces los vacios son -1 en lugar de \0
                    if (bf.b_content[j] != '\0' && bf.b_content[j] != -1 && bf.b_content[j] != (char)255) {
                        textContent += bf.b_content[j];
                    }
                }
            }
        }
        fclose(file);
    }

    std::cout << "DEBUG FILE: Contenido extraido -> [" << textContent << "]" << std::endl;

    std::string cleanJsonContent = "";
    for (char c : textContent) {
        if (c == '"') cleanJsonContent += "\\\"";
        else if (c == '\n') cleanJsonContent += "\\n";
        else if (c == '\\') cleanJsonContent += "\\\\";
        else if (c >= 32 && c <= 126) cleanJsonContent += c; // Solo ASCII imprimible
    }

    std::cout << "=== FIN DEBUG FILE ===\n" << std::endl;
    return "{\"content\":\"" + cleanJsonContent + "\"}";
}