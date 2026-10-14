#include "WebEncoding.h"
#include <cassert>
#include <iostream>

int main() {
  using chime::acceptsGzip;
  assert(acceptsGzip(nullptr));
  assert(acceptsGzip(""));
  for (const char *header : {"gzip", "gzip, deflate, br", "GZip", " GZIP ; Q=0.5 ",
      "br;q=1, gzip;q=0.1", "*", "*;q=0.5", "*;q=0, gzip;q=1", "gzip;unknown=value"})
    assert(acceptsGzip(header));
  for (const char *header : {"identity", "br, deflate", "xgzip", "gzipx", "gzip;q=0",
      "gzip;q=0.0, *;q=1", "*;q=0", "gzip;q=", "gzip;q=bogus", "gzip;q=-1",
      "gzip;q=2", "gzip;q=nan", "gzip;q=inf", "gzip;q=1x", "gzip;q", ", ,"})
    assert(!acceptsGzip(header));
  std::cout << "HTTP encoding tests passed: gzip negotiation, quality values, wildcards and rejection\n";
}
