#ifndef SILVEROS_V8_RUNTIME_MGR_H
#define SILVEROS_V8_RUNTIME_MGR_H

#ifdef __cplusplus
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>
#include "v8.h"
#include <memory>
#include <string>

namespace silveros {

class V8Runtime {
public:
    V8Runtime();
    ~V8Runtime();

    bool Initialize();
    bool ExecuteScript(const std::string& source, const std::string& name = "script.js");

private:
    static void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void ReadChar(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void GetVersion(const v8::FunctionCallbackInfo<v8::Value>& args);

    v8::Isolate* isolate_;
    v8::Global<v8::Context> context_;
};

} // namespace silveros

extern "C" {
#endif

// C wrappers for kernel.c
void* create_v8_runtime(void);
bool v8_execute_script(void* runtime_ptr, const char* source, const char* name);

#ifdef __cplusplus
}
#endif

#endif
