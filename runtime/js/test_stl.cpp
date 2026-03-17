#include <memory>
namespace v8 { 
    class JobHandle {}; 
    enum class TaskPriority { kUserBlocking };
    class Isolate {};
    class TaskRunner {};
}
namespace v8::platform::silver {
    class SilverTaskRunner : public v8::TaskRunner {};
}

std::unique_ptr<v8::JobHandle> f() { return nullptr; }

int main() {
    std::shared_ptr<v8::platform::silver::SilverTaskRunner> runner;
    std::shared_ptr<v8::TaskRunner> base_runner = runner;
    return 0;
}
