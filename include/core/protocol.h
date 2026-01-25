#pragma once
#include <string_view>

class SBBF;

namespace protocol {

void handle_line(SBBF& store, std::string_view line, std::string& out);

}