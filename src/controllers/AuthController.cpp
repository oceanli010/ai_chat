#include "AuthController.h"

std::shared_ptr<EmailSender> AuthController::email_sender_ = nullptr;
