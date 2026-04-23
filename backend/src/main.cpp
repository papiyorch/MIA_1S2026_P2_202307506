#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "command_handler.h"
#include "command_parser.h"

// Función para dividir comandos por líneas
std::vector<std::string> splitCommands(const std::string& input) {
    std::vector<std::string> commands;
    std::stringstream ss(input);
    std::string line;
    
    while (std::getline(ss, line)) {
        // Eliminar espacios en blanco al inicio y final
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Ignorar líneas vacías y comentarios
        if (!line.empty() && line[0] != '#') {
            commands.push_back(line);
        }
    }
    
    return commands;
}

// Función para ejecutar comandos
std::string executeCommands(const std::string& input) {
    CommandHandler handler;
    std::vector<std::string> commands = splitCommands(input);
    
    std::string result;
    
    for (const auto& command : commands) {
        result += "> " + command + "\n";
        result += handler.processCommand(command) + "\n\n";
    }
    
    return result;
}

// Función para procesar petición HTTP manual (sin dependencias externas)
int startServer() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::cerr << "Error: No se pudo crear el socket" << std::endl;
        return 1;
    }
    
    // Permitir reutilizar el puerto
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error: No se pudo hacer bind al puerto 8000" << std::endl;
        return 1;
    }
    
    listen(server_sock, 5);
    std::cout << "✓ Servidor levantado en http://localhost:8000" << std::endl;
    std::cout << "Escuchando conexiones..." << std::endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;
        
        char buffer[65536] = {0};
        ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        
        std::string request(buffer);
        std::string response;
        
        // Verificar si es una petición POST a /execute
        if (request.find("POST /execute") != std::string::npos) {
            // Extraer el body JSON
            size_t body_start = request.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                std::string body = request.substr(body_start + 4);
                
                // Simple parser de JSON mejorado
                size_t start = body.find("\"commands\":");
                if (start != std::string::npos) {
                    start = body.find("\"", start + 11);  // Comilla inicial
                    
                    // Buscar comilla final, saltando comillas escapadas
                    size_t end = start + 1;
                    while (end < body.length()) {
                        if (body[end] == '\\' && end + 1 < body.length()) {
                            end += 2;  // Saltar carácter escapado
                        } else if (body[end] == '"') {
                            break;  // Encontramos la comilla final
                        } else {
                            end++;
                        }
                    }
                    
                    if (start != std::string::npos && end < body.length()) {
                        std::string commands = body.substr(start + 1, end - start - 1);
                        
                        // Convertir secuencias escape de JSON a formato real
                        std::string converted;
                        for (size_t i = 0; i < commands.length(); ++i) {
                            if (commands[i] == '\\' && i + 1 < commands.length()) {
                                char next = commands[i + 1];
                                if (next == 'n') {
                                    converted += '\n';
                                    ++i;
                                } else if (next == 'r') {
                                    converted += '\r';
                                    ++i;
                                } else if (next == 't') {
                                    converted += '\t';
                                    ++i;
                                } else if (next == '"') {
                                    converted += '"';
                                    ++i;
                                } else if (next == '\\') {
                                    converted += '\\';
                                    ++i;
                                } else {
                                    converted += commands[i];
                                }
                            } else {
                                converted += commands[i];
                            }
                        }
                        
                        std::string output = executeCommands(converted);
                        
                        // Escapar JSON - el orden es crítico
                        std::string escaped_output;
                        for (char c : output) {
                            if (c == '\\') escaped_output += "\\\\";
                            else if (c == '"') escaped_output += "\\\"";
                            else if (c == '\n') escaped_output += "\\n";
                            else if (c == '\r') escaped_output += "\\r";
                            else escaped_output += c;
                        }
                        
                        response = "HTTP/1.1 200 OK\r\n";
                        response += "Content-Type: application/json;charset=utf-8\r\n";
                        response += "Access-Control-Allow-Origin: *\r\n";
                        response += "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
                        response += "Access-Control-Allow-Headers: Content-Type\r\n";
                        response += "Connection: close\r\n";
                        
                        std::string json_body = "{\"output\":\"" + escaped_output + "\"}";
                        response += "Content-Length: " + std::to_string(json_body.length()) + "\r\n";
                        response += "\r\n";
                        response += json_body;
                    }
                }
            }
        } else if (request.find("OPTIONS /execute") != std::string::npos) {
            // Responder a preflight requests
            response = "HTTP/1.1 200 OK\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Content-Length: 0\r\n";
            response += "Connection: close\r\n";
            response += "\r\n";
        } else if (request.find("GET /health") != std::string::npos) {
            std::string health_json = "{\"status\":\"ok\"}";
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(health_json.length()) + "\r\n";
            response += "Connection: close\r\n";
            response += "\r\n";
            response += health_json;
        } else {
            response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Length: 0\r\n";
            response += "Connection: close\r\n";
            response += "\r\n";
        }
        
        if (!response.empty()) {
            // Enviar respuesta completa, manejando respuestas grandes
            size_t total_sent = 0;
            while (total_sent < response.length()) {
                ssize_t sent = send(client_sock, response.c_str() + total_sent, 
                                   response.length() - total_sent, 0);
                if (sent < 0) break;
                total_sent += sent;
            }
        }
        close(client_sock);
    }
    
    close(server_sock);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--server") {
        return startServer();
    }
    
    CommandHandler handler;
    std::string line;
    
    std::cout << "=== ExtreamFS - Sistema de Archivos EXT2 ===" << std::endl;
    std::cout << "Escribe comandos (exit para salir):" << std::endl;
    std::cout << std::endl;
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        if (CommandParser::toLower(line) == "exit") break;
        
        std::string result = handler.processCommand(line);
        std::cout << result << std::endl;
    }
    
    return 0;
}
