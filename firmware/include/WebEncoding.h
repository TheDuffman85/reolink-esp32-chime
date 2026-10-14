#pragma once

namespace chime {
// An absent Accept-Encoding permits any coding. Explicit gzip preferences
// override a wildcard, including gzip;q=0.
bool acceptsGzip(const char *acceptEncoding);
}
