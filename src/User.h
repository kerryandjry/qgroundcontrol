//
// Created by lab509 on 9/28/23.
//

#ifndef QGROUNDCONTROL_USER_H
#define QGROUNDCONTROL_USER_H

#endif //QGROUNDCONTROL_USER_H

#pragma once
#include <string>
#include <iostream>

struct User {
private:
    std::string username;
    std::string password;
public:
    auto userEnterUsername();

    auto userEnterPassword();
};