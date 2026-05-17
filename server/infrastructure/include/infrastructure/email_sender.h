#pragma once

#include <string>

class EmailSender {
public:
    bool send(const std::string& to, const std::string& subject, const std::string& body);
};