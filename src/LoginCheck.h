#include <filesystem>
#include <fstream>
#include <iostream>

bool loginCheck(const std::string &username, const std::string &password) {
  auto current_path = std::filesystem::current_path();
  auto file = "passport_list.yaml";

  while (!current_path.empty()) {
    if (std::filesystem::exists(current_path / file)) {
      break;
    }
    current_path = current_path.parent_path();
    // std::cerr << "File does not exist:" << file << std::endl;
    // return false;
  }

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
