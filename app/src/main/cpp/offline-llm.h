#pragma once

#include <string>

namespace jarvis {

// Uses llama.cpp to run the locally downloaded Qwen3 GGUF model.
bool offline_llm_model_exists(const std::string& modelPath);
std::string offline_llm_generate(const std::string& modelPath,
                                 const std::string& userText,
                                 std::string* error);
void offline_llm_shutdown();

}
