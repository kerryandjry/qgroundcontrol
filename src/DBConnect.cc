#include "DBConnect.h"

#include <utility>
#include <fstream>
#include <streambuf>
#include <dirent.h>

DBConnect::DBConnect(std::string ip, std::string port):
        _ip(std::move(ip)), _port(std::move(port)) {
    _connection_string = "mongodb://" + _ip + ":" + _port;
}
bool DBConnect::LoginCheck(const std::string &username, const std::string &password) {
    auto find_one_result = collection.find_one(make_document(kvp("username", username)));
    if (!find_one_result) {
        Register(username, password);
        return true;
    }
    auto pw = find_one_result.value().view()["password"].get_string().value.to_string();
    if (pw == password) return true;
    this->_username = username;
    assert(doc["_id"].type() == bsoncxx::type::k_oid);
    return false;
}

bool DBConnect::Register(const std::string &username, const std::string &password) {
    auto insert_one_result = collection.insert_one(make_document(kvp("username", username), kvp("password", password)));
    // TODO: Add register UI and logic
    return false;
}

bool DBConnect::UploadFile(const std::string &path) {
    std::ifstream file {path};
    std::vector<uint8_t> data {(std::istream_iterator<char>(file)), std::istream_iterator<char>()};

    auto bucket = db.gridfs_bucket();
    auto uploader = bucket.open_upload_stream("log.txt");
    file.close();
    return false;
}

bool DBConnect::UploadLog(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    struct dirent *entry;
    bsoncxx::builder::stream::document document{};
    while ((entry = readdir(dir)) != nullptr) {
        std::string file_path = path + "/" + entry->d_name;
        std::cout << file_path << std::endl;
        std::ifstream file{file_path};
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            document << "filename" << file_path << "content" << content;
            auto insert_one_result = collection.insert_one(document.view());
            assert(insert_one_result);
            std::cout << "upload logs : "<< file_path << "successfully" << std::endl;
        }
        file.close();
    }
}