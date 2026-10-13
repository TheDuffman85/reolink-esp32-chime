// Runs a fuzz target over corpus files or directories without libFuzzer, so
// tools/test.sh can replay the corpus with g++ and AddressSanitizer/UBSan.
#include <dirent.h>
#include <sys/stat.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

namespace {
size_t replay(const std::string &path) {
  struct stat info;
  if (stat(path.c_str(), &info) != 0) { std::perror(path.c_str()); return 0; }
  if (S_ISDIR(info.st_mode)) {
    size_t count = 0;
    if (DIR *dir = opendir(path.c_str())) {
      while (const dirent *entry = readdir(dir))
        if (entry->d_name[0] != '.') count += replay(path + "/" + entry->d_name);
      closedir(dir);
    }
    return count;
  }
  std::ifstream file(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
  return 1;
}
}

int main(int argc, char **argv) {
  size_t count = 0;
  for (int i = 1; i < argc; ++i) count += replay(argv[i]);
  std::printf("%zu inputs\n", count);
  return count ? 0 : 1;
}
