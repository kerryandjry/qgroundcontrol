#include <fstream>
#include <iostream>

bool loginCheck(const std::string &username, const std::string &password) {
  auto file = "passport_list.yaml";

  std::ifstream infile(file);

  if (!infile.is_open()) {
    std::cerr << "Unable to open file" << file << std::endl;
    return false;
  }
  std::string line;
  while (std::getline(infile, line)) {
    auto fine_pos = line.find(' ');
    auto name = line.substr(0, fine_pos);
    if (name == username) {
      auto pw = line.substr(fine_pos + 1);
      if (pw == password) {
        infile.close();
        return true;
      }
    }
  }
  infile.close();
  return false;
}
