#pragma once

#include <sstream>
#include <string_view>
#include "models/request.h"

class Parser {
public:
    static bool parse(std::string_view raw, HttpRequest& req) {
        // Find request line end
        size_t line_end = raw.find("\r\n");
        if (line_end == std::string_view::npos) return false;

        std::string_view line = raw.substr(0, line_end);

        // method SP path SP version
        size_t sp1 = line.find(' ');
        if (sp1 == std::string_view::npos) return false;

        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos) return false;

        req.method  = line.substr(0, sp1);
        req.path    = line.substr(sp1 + 1, sp2 - (sp1 + 1));
        req.version = line.substr(sp2 + 1);

        return true;
    }
};