#include "../../include/v8_runtime.h"
#include "../../include/serial.h"
#include "../../include/timer.h"
#include "../../include/keyboard.h"
#include "../../include/io.h"
#include "v8-isolate.h"
#include "v8-context.h"
#include "v8-template.h"
#include "v8-function-callback.h"
#include "v8-script.h"
#include "v8-primitive.h"
#include "v8-object.h"
#include "v8-array-buffer.h"

namespace silveros {

class SilverArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
public:
    void* Allocate(size_t length) override {
        void* data = new char[length];
        for (size_t i = 0; i < length; i++) ((char*)data)[i] = 0;
        return data;
    }
    void* AllocateUninitialized(size_t length) override {
        return new char[length];
    }
    void Free(void* data, size_t length) override {
        (void)length;
        delete[] (char*)data;
    }
};

V8Runtime::V8Runtime() : isolate_(nullptr) {}

V8Runtime::~V8Runtime() {
    if (isolate_) {
        isolate_->Dispose();
    }
}

bool V8Runtime::Initialize() {
    v8::Isolate::CreateParams create_params;
    static SilverArrayBufferAllocator allocator;
    create_params.array_buffer_allocator = &allocator;
    
    isolate_ = v8::Isolate::New(create_params);
    if (!isolate_) return false;

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);

    // Create global template
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    
    // Bindings
    global->Set(isolate_, "print", v8::FunctionTemplate::New(isolate_, Print));
    global->Set(isolate_, "println", v8::FunctionTemplate::New(isolate_, Print));
    global->Set(isolate_, "readChar", v8::FunctionTemplate::New(isolate_, ReadChar));

    // Create os object
    v8::Local<v8::ObjectTemplate> os = v8::ObjectTemplate::New(isolate_);
    os->Set(isolate_, "version", v8::FunctionTemplate::New(isolate_, GetVersion));
    global->Set(isolate_, "os", os);

    v8::Local<v8::Context> context = v8::Context::New(isolate_, nullptr, global);
    context_.Reset(isolate_, context);

    return true;
}

bool V8Runtime::ExecuteScript(const std::string& source, const std::string& name) {
    if (!isolate_) return false;
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = context_.Get(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> v8_source = v8::String::NewFromUtf8(isolate_, source.c_str()).ToLocalChecked();
    v8::Local<v8::String> v8_name = v8::String::NewFromUtf8(isolate_, name.c_str()).ToLocalChecked();

    v8::ScriptOrigin origin(v8_name);
    v8::MaybeLocal<v8::Script> script = v8::Script::Compile(context, v8_source, &origin);
    
    if (script.IsEmpty()) return false;

    v8::MaybeLocal<v8::Value> result = script.ToLocalChecked()->Run(context);
    return !result.IsEmpty();
}

void V8Runtime::Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope handle_scope(isolate);
    
    if (args.Length() < 1) return;
    
    v8::String::Utf8Value str(isolate, args[0]);
    if (*str) {
        serial_printf("%s", *str);
    }
}

void V8Runtime::ReadChar(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope handle_scope(isolate);
    
    char ch = 0;
    while (!(ch = keyboard_getchar_nb())) {
        hlt(); 
    }
    args.GetReturnValue().Set(v8::Integer::New(isolate, ch));
}

void V8Runtime::GetVersion(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope handle_scope(isolate);
    
    args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "SilverOS v0.1.0 (JS Mode)").ToLocalChecked());
}

} // namespace silveros

extern "C" void* create_v8_runtime() {
    silveros::V8Runtime* runtime = new silveros::V8Runtime();
    if (runtime->Initialize()) {
        return (void*)runtime;
    }
    delete runtime;
    return nullptr;
}

extern "C" bool v8_execute_script(void* runtime_ptr, const char* source, const char* name) {
    if (!runtime_ptr) return false;
    silveros::V8Runtime* runtime = (silveros::V8Runtime*)runtime_ptr;
    std::string s_source(source);
    std::string s_name(name ? name : "script.js");
    return runtime->ExecuteScript(s_source, s_name);
}
