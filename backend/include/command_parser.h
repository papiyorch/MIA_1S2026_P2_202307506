#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <map>
#include <vector>

class CommandParser {
public:
    // Parsea una línea de comando
    static std::map<std::string, std::string> parseCommand(const std::string& line);
    
    // Obtiene el nombre del comando (primer token)
    static std::string getCommandName(const std::string& line);
    
    // Valida si un parámetro existe en el mapa
    static bool hasParameter(const std::map<std::string, std::string>& params, 
                        const std::string& param);
    
    // Obtiene el valor de un parámetro
    static std::string getParameter(const std::map<std::string, std::string>& params,
                                const std::string& param,
                                const std::string& defaultValue = "");
    
    // Convierte string a minúsculas
    static std::string toLower(const std::string& str);
    
    // Limpia espacios en blanco
    static std::string trim(const std::string& str);
};

#endif // COMMAND_PARSER_H
