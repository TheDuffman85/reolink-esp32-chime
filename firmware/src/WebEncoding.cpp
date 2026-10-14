#include "WebEncoding.h"
#include <cstdlib>

namespace chime {
namespace {
bool space(char c) { return c == ' ' || c == '\t'; }
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; }
bool token(const char *start, const char *end, const char *expected) {
  while (start < end && space(*start)) ++start;
  while (end > start && space(end[-1])) --end;
  while (start < end && *expected && lower(*start) == *expected) { ++start; ++expected; }
  return start == end && !*expected;
}
}
bool acceptsGzip(const char *header) {
  if (!header || !*header) return true;
  bool gzipSeen = false, gzipAllowed = false, wildcard = false;
  while (*header) {
    const char *end = header;
    while (*end && *end != ',') ++end;
    const char *codingEnd = header;
    while (codingEnd < end && *codingEnd != ';') ++codingEnd;
    bool allowed = true;
    for (const char *parameter = codingEnd; parameter < end;) {
      const char *start = ++parameter;
      while (parameter < end && *parameter != ';') ++parameter;
      const char *equals = start;
      while (equals < parameter && *equals != '=') ++equals;
      if (!token(start, equals, "q")) continue;
      if (equals == parameter) { allowed = false; continue; }
      char *numberEnd;
      const double quality = std::strtod(equals + 1, &numberEnd);
      while (numberEnd < parameter && space(*numberEnd)) ++numberEnd;
      allowed = numberEnd > equals + 1 && numberEnd == parameter && quality > 0 && quality <= 1;
    }
    if (token(header, codingEnd, "gzip")) { gzipSeen = true; gzipAllowed = allowed; }
    else if (token(header, codingEnd, "*")) wildcard = allowed;
    header = *end ? end + 1 : end;
  }
  return gzipSeen ? gzipAllowed : wildcard;
}
}
