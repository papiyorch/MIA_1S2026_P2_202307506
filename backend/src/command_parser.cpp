#include "command_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

std::map<std::string, std::string> CommandParser::parseCommand(const std::string& line) {
    std::map<std::string, std::string> params;
    std::string trimmedLine = trim(line);
    
    // Saltar el comando (primer token)
    size_t firstSpace = trimmedLine.find(' ');
    if (firstSpace == std::string::npos) {
        return params; // Solo hay comando, sin parámetros
    }
    
    std::string paramsStr = trimmedLine.substr(firstSpace + 1);
    
    size_t pos = 0;
    while (pos < paramsStr.length()) {
        // Saltar espacios
        while (pos < paramsStr.length() && std::isspace(paramsStr[pos])) {
            pos++;
        }
        
        if (pos >= paramsStr.length()) break;
        
        // Encontrar el parámetro (comienza con -)
        if (paramsStr[pos] != '-') {
            pos++;
            continue;
        }
        
        pos++; // Saltar el -
        
        // Buscar el = o espacio
        size_t equalPos = paramsStr.find('=', pos);
        size_t spacePos = paramsStr.find(' ', pos);
        
        size_t endPos = std::string::npos;
        if (equalPos != std::string::npos && spacePos != std::string::npos) {
            endPos = std::min(equalPos, spacePos);
        } else if (equalPos != std::string::npos) {
            endPos = equalPos;
        } else if (spacePos != std::string::npos) {
            endPos = spacePos;
        } else {
            endPos = paramsStr.length();
        }
        
        // Extraer nombre del parámetro
        std::string paramName = paramsStr.substr(pos, endPos - pos);
        paramName = toLower(paramName);
        
        pos = endPos;
        
        // Verificar si hay valor
        std::string paramValue = "";
        if (pos < paramsStr.length() && paramsStr[pos] == '=') {
            pos++; // Saltar el =
            
            // Verificar si el valor está entre comillas
            if (pos < paramsStr.length() && paramsStr[pos] == '"') {
                pos++; // Saltar la comilla inicial
                size_t endQuote = paramsStr.find('"', pos);
                if (endQuote != std::string::npos) {
                    paramValue = paramsStr.substr(pos, endQuote - pos);
                    pos = endQuote + 1;
                } else {
                    paramValue = paramsStr.substr(pos);
                    pos = paramsStr.length();
                }
            } else {
                // Valor sin comillas
                size_t paramEndPos = paramsStr.find(' ', pos);
                if (paramEndPos == std::string::npos) {
                    paramValue = paramsStr.substr(pos);
                    pos = paramsStr.length();
                } else {
                    paramValue = paramsStr.substr(pos, paramEndPos - pos);
                    pos = paramEndPos;
                }
            }
        }
        
        params[paramName] = paramValue;
    }
    
    return params;
}

std::string CommandParser::getCommandName(const std::string& line) {
    std::string trimmed = trim(line);
    size_t space = trimmed.find(' ');
    if (space == std::string::npos) {
        return toLower(trimmed);
    }
    return toLower(trimmed.substr(0, space));
}

bool CommandParser::hasParameter(const std::map<std::string, std::string>& params,
                                const std::string& param) {
    return params.find(toLower(param)) != params.end();
}

std::string CommandParser::getParameter(const std::map<std::string, std::string>& params,
                                        const std::string& param,
                                        const std::string& defaultValue) {
    std::string lowerParam = toLower(param);
    auto it = params.find(lowerParam);
    if (it != params.end()) {
        return it->second;
    }
    return defaultValue;
}

std::string CommandParser::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string CommandParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}
