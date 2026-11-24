#include "offline-llm.h"

#include <llama.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::mutex g_llmMutex;
llama_model* g_model = nullptr;
llama_context* g_context = nullptr;
const llama_vocab* g_vocab = nullptr;

bool load_model_locked(const std::string& modelPath, std::string* error) {
    if (g_model && g_context && g_vocab) return true;

    llama_backend_init();

    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = 0;

    g_model = llama_model_load_from_file(modelPath.c_str(), modelParams);
    if (!g_model) {
        if (error) *error = "Offline model could not be loaded. Check free RAM and model file.";
        return false;
    }

    g_vocab = llama_model_get_vocab(g_model);
    llama_context_params contextParams = llama_context_default_params();
    contextParams.n_ctx = 1536;
    contextParams.n_batch = 1024;
    contextParams.n_ubatch = 256;
    contextParams.n_threads = 3;
    contextParams.n_threads_batch = 3;
    contextParams.no_perf = true;

    g_context = llama_init_from_model(g_model, contextParams);
    if (!g_context) {
        if (error) *error = "Could not allocate the local AI context.";
        llama_model_free(g_model);
        g_model = nullptr;
        g_vocab = nullptr;
        return false;
    }
    return true;
}
}

namespace jarvis {

bool offline_llm_model_exists(const std::string& modelPath) {
    FILE* file = std::fopen(modelPath.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fclose(file);
    return size > 450L * 1024L * 1024L;
}

std::string offline_llm_generate(const std::string& modelPath,
                                 const std::string& userText,
                                 std::string* error) {
    std::lock_guard<std::mutex> lock(g_llmMutex);
    if (!load_model_locked(modelPath, error)) return {};

    std::string safeQuestion = userText;
    if (safeQuestion.size() > 1200) safeQuestion.resize(1200);

    const std::string prompt =
        "<|im_start|>system\n"
        "You are JARVIS, a concise helpful personal assistant. Do not reveal internal reasoning. "
        "Answer in natural Hindi. Understand Hindi and Hinglish. "
        "Do not claim you performed actions unless the app confirms them. "
        "Keep spoken answers short and practical.<|im_end|>\n"
        "<|im_start|>user\n" + safeQuestion + "<|im_end|>\n"
        "<|im_start|>assistant\n";

    const int32_t required = llama_tokenize(
        g_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
        nullptr, 0, true, true);
    if (required == INT32_MIN || required >= 1200) {
        if (error) *error = "The spoken question is too long for the local model context.";
        return {};
    }

    const int32_t tokenCount = required < 0 ? -required : required;
    std::vector<llama_token> tokens(static_cast<size_t>(tokenCount));
    const int32_t actual = llama_tokenize(
        g_vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
        tokens.data(), tokenCount, true, true);
    if (actual < 0) {
        if (error) *error = "Could not tokenize the local AI prompt.";
        return {};
    }

    llama_memory_clear(llama_get_memory(g_context), true);
    llama_batch batch = llama_batch_init(tokenCount, 0, 1);
    if (!batch.token || !batch.pos || !batch.n_seq_id ||
        !batch.seq_id || !batch.logits) {
        llama_batch_free(batch);
        if (error) *error = "Could not allocate the model input batch.";
        return {};
    }

    for (int32_t i = 0; i < tokenCount; ++i) {
        batch.token[i] = tokens[static_cast<size_t>(i)];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == tokenCount - 1) ? 1 : 0;
    }
    batch.n_tokens = tokenCount;

    if (llama_decode(g_context, batch) != 0) {
        llama_batch_free(batch);
        if (error) *error = "Local AI prompt evaluation failed.";
        return {};
    }

    llama_sampler_chain_params samplerParams = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(samplerParams);
    if (!sampler) {
        llama_batch_free(batch);
        if (error) *error = "Could not create the local AI sampler.";
        return {};
    }
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.90f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.70f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string response;
    response.reserve(2048);

    for (int32_t generated = 0; generated < 120; ++generated) {
        const llama_token token = llama_sampler_sample(sampler, g_context, -1);
        llama_sampler_accept(sampler, token);
        if (llama_vocab_is_eog(g_vocab, token)) break;

        char piece[256];
        const int32_t count = llama_token_to_piece(
            g_vocab, token, piece, sizeof(piece), 0, false);
        if (count > 0) response.append(piece, static_cast<size_t>(count));
        if (response.size() > 4000) break;

        batch.token[0] = token;
        batch.pos[0] = tokenCount + generated;
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;
        batch.logits[0] = 1;
        batch.n_tokens = 1;
        if (llama_decode(g_context, batch) != 0) break;
    }

    llama_sampler_free(sampler);
    llama_batch_free(batch);

    // Trim accidental leading/trailing whitespace and Qwen reasoning markers.
    const size_t begin = response.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const size_t end = response.find_last_not_of(" \t\r\n");
    response = response.substr(begin, end - begin + 1);
    return response;
}

void offline_llm_shutdown() {
    std::lock_guard<std::mutex> lock(g_llmMutex);
    if (g_context) llama_free(g_context);
    if (g_model) llama_model_free(g_model);
    g_context = nullptr;
    g_model = nullptr;
    g_vocab = nullptr;
}

}
