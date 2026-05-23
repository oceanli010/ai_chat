#include "ChatController.h"

std::string ChatController::ai_api_url_ = "https://api.openai.com/v1/chat/completions";
std::string ChatController::ai_api_key_ = "";
std::string ChatController::ai_model_ = "gpt-3.5-turbo";
