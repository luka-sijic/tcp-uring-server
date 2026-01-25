#include "protocol.h"
#include "sbbf.h"

#include <cctype>
#include <string>

static inline std::string_view trim(std::string_view s) {
  while (!s.empty() &&
         (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
    s.remove_prefix(1);
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

void protocol::handle_line(SBBF &store, std::string_view line,
                           std::string &out) {
  line = trim(line);
  if (line.empty())
    return;

  auto sp = line.find(' ');
  std::string_view cmd =
      (sp == std::string_view::npos) ? line : line.substr(0, sp);
  std::string_view val = (sp == std::string_view::npos)
                             ? std::string_view{}
                             : trim(line.substr(sp + 1));

  int ans = 0;
  if (cmd == "insert") {
    ans = store.insert(val);
  } else if (cmd == "contains") {
    ans = store.possiblyContains(val);
  } else {
    ans = 0; // unknown command
  }

  out.append(std::to_string(ans));
  out.push_back('\n');
}
