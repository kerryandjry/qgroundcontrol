#include "DBConnect.h"

#include <utility>
#include <fstream>
#include <streambuf>
#include <tinydir.h>
#include <chrono>

DBConnect::DBConnect(std::string ip, std::string port, std::string log_path):
        _ip(std::move(ip)), _port(std::move(port)), log_path(std::move(log_path)) {
    _connection_string = "mongodb://" + _ip + ":" + _port;
}
bool DBConnect::LoginCheck(const std::string &username, const std::string &password) {
    auto find_one_result = collection_users.find_one(make_document(kvp("username", username)));
    if (!find_one_result) {
        Register(username, password);
        return true;
    }
    auto pw = find_one_result.value().view()["password"].get_string().value.to_string();
    if (pw == password) return true;
    assert(doc["_id"].type() == bsoncxx::type::k_oid);
    return false;
}

bool DBConnect::Register(const std::string &username, const std::string &password) {
    auto insert_one_result = collection_users.insert_one(make_document(kvp("username", username), kvp("password", password)));
    assert(insert_one_result);
    // TODO: Add register UI and logic
    return false;
}

bool DBConnect::UploadLog(const std::string& path) {
    tinydir_dir tiny_dir;
    tinydir_open(&tiny_dir, path.c_str());
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    while (tiny_dir.has_next) {
        tinydir_file file;
        tinydir_readfile(&tiny_dir, &file);
        if (file.name[0] == '.') {
            tinydir_next(&tiny_dir);
            continue;
        }
        if (file.is_dir) {
            UploadLog(file.path);
            tinydir_next(&tiny_dir);
            continue;
        }
        std::string file_path = path + "/" + file.name;
        std::cout << "file_path : " << file_path << std::endl;
        std::ifstream in{file.path, std::ios::binary};
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bsoncxx::builder::stream::document document{};
        auto insert_one_result = collection_logs.insert_one(make_document(kvp("file", file.name), kvp("log_content", content), kvp("time", now_ms.count())));
        assert(insert_one_result);
        std::cout << "upload logs : "<< file.name << " successfully" << std::endl;

//        DeleteFile(file_path);
        tinydir_next(&tiny_dir);
    }
    tinydir_close(&tiny_dir);
}

bool DBConnect::DeleteFile() {
    int delete_result = remove(log_path.c_str());
    if (delete_result == 0) std::cout << "delete file : " << log_path << " successfully" << std::endl;
    else std::cerr << "delete file : " << log_path << " failed" << std::endl;

    return false;
}

bool DBConnect::UploadGPS() {
    std::string gps_location = "24.123456, 121.123456";
    auto insert_one_result = collection_gps.insert_one(make_document(kvp("is_run", 1), kvp("gps_location", gps_location)));
    return false;
}
