#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <filesystem> 
#include "command_handler.h"
#include "command_parser.h"
#include "structures.h" 

namespace fs = std::filesystem;

// --- FUNCIONES DE APOYO PARA EL VISUALIZADOR ---

std::string getDisksJSON() {
    std::string json = "[";
    bool first = true;
    std::string path = "/tmp"; 

    try {
        if (fs::exists(path)) {
            auto options = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::directory_iterator(path, options)) {
                try {
                    if (entry.is_regular_file() && entry.path().extension() == ".mia") {
                        if (!first) json += ",";
                        json += "\"" + entry.path().filename().string() + "\"";
                        first = false;
                    }
                } catch (...) {
                    continue;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error crítico en getDisksJSON: " << e.what() << std::endl;
    }
    json += "]";
    return json;
}

std::string getPartitionsJSON(std::string diskName) {
    std::string json = "[";
    bool first = true;
    
    std::string fullPath = "/tmp/" + diskName;

    std::cout << "\n=== DEBUG: Buscando particiones en: " << fullPath << " ===" << std::endl;

    FILE* file = fopen(fullPath.c_str(), "rb");
    if (file) {
        MBR mbr;
        size_t leidos = fread(&mbr, sizeof(MBR), 1, file);
        
        if (leidos == 1) {
            std::cout << "DEBUG: MBR leido correctamente." << std::endl;
            
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_s > 0 && mbr.mbr_partitions[i].part_start >= 157) {
                    char safeName[17] = {0}; 
                    std::strncpy(safeName, mbr.mbr_partitions[i].part_name, 16);
                    std::string pName(safeName);

                    if (!pName.empty()) {
                        std::cout << ">>> DEBUG: PARTICION VALIDA -> " << pName << std::endl;
                        if (!first) json += ",";
                        json += "\"" + pName + "\"";
                        first = false;
                    }
                }
            }
        } else {
            std::cout << "DEBUG: ERROR. No se pudieron leer los 157 bytes del MBR." << std::endl;
        }
        fclose(file);
    } else {
        std::cout << "DEBUG: ERROR CRITICO. No se pudo abrir el archivo en " << fullPath << std::endl;
    }
    
    json += "]";
    return json;
}

// --- LÓGICA DEL SERVIDOR ---

std::vector<std::string> splitCommands(const std::string& input) {
    std::vector<std::string> commands;
    std::stringstream ss(input);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty() && line[0] != '#') commands.push_back(line);
    }
    return commands;
}

int startServer() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) return 1;
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) return 1;
    
    listen(server_sock, 5);
    std::cout << "✓ Servidor levantado en http://localhost:8000" << std::endl;
    
    CommandHandler handler; 

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;
        
        char buffer[65536] = {0};
        recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        std::string request(buffer);
        std::string response;
        
        if (request.find("POST /execute") != std::string::npos) {
            size_t body_start = request.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                std::string body = request.substr(body_start + 4);
                size_t start = body.find("\"commands\":");
                if (start != std::string::npos) {
                    start = body.find("\"", start + 11);
                    size_t end = start + 1;
                    while (end < body.length() && body[end] != '"') {
                        if (body[end] == '\\') end += 2; else end++;
                    }
                    if (end < body.length()) {
                        std::string cmdRaw = body.substr(start + 1, end - start - 1);
                        std::string clean = "";
                        for(size_t i=0; i<cmdRaw.length(); i++){
                            if(cmdRaw[i]=='\\' && i+1<cmdRaw.length() && cmdRaw[i+1]=='n'){ clean+='\n'; i++; }
                            else clean+=cmdRaw[i];
                        }
                        
                        std::vector<std::string> cmds = splitCommands(clean);
                        std::string output = "";
                        for(const auto& c : cmds) output += "> " + c + "\n" + handler.processCommand(c) + "\n\n";
                        
                        std::string escaped = "";
                        for(char c : output) {
                            if(c=='\\') escaped+="\\\\"; else if(c=='"') escaped+="\\\"";
                            else if(c=='\n') escaped+="\\n"; else escaped+=c;
                        }
                        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"output\":\""+escaped+"\"}";
                    }
                }
            }
        }
        else if (request.find("GET /api/disks") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n" + getDisksJSON();
        }
        else if (request.find("GET /api/partitions") != std::string::npos) {
            size_t pos = request.find("disk=");
            std::string dName = (pos != std::string::npos) ? request.substr(pos + 5, request.find_first_of(" &", pos) - (pos + 5)) : "";
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n" + getPartitionsJSON(dName);
        }
        else if (request.find("GET /api/explore") != std::string::npos) {
            auto getParam = [&](std::string p) {
                size_t pos = request.find(p + "=");
                if (pos == std::string::npos) return std::string("");
                size_t end = request.find_first_of(" &", pos);
                return request.substr(pos + p.length() + 1, end - (pos + p.length() + 1));
            };
            
            std::string d = getParam("disk");
            std::string p = getParam("partition");
            std::string path = getParam("path");

            std::string body = handler.getDirectoryJSON("/tmp/" + d, p, path);
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n" + body;
        }
        // ==========================================
        // NUEVO ENDPOINT: LEER CONTENIDO DE ARCHIVO
        // ==========================================
        else if (request.find("GET /api/file") != std::string::npos) {
            auto getParam = [&](std::string p) {
                size_t pos = request.find(p + "=");
                if (pos == std::string::npos) return std::string("");
                size_t end = request.find_first_of(" &", pos);
                return request.substr(pos + p.length() + 1, end - (pos + p.length() + 1));
            };
            
            std::string d = getParam("disk");
            std::string p = getParam("partition");
            std::string path = getParam("path");

            // Llama a la función que debes tener en command_handler.cpp
            std::string body = handler.getFileContentJSON("/tmp/" + d, p, path);
            
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n" + body;
        }
        // ==========================================
        else if (request.find("OPTIONS") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\n\r\n";
        }
        else if (request.find("GET /health") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
        }
        
        send(client_sock, response.c_str(), response.length(), 0);
        close(client_sock);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--server") return startServer();
    
    CommandHandler handler;
    std::string line;
    std::cout << "=== ExtreamFS - Servidor Activo ===\n";
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line) || line == "exit") break;
        if (line.empty()) continue;
        std::cout << handler.processCommand(line) << std::endl;
    }
    return 0;
}