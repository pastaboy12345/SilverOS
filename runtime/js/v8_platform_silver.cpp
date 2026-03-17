#include "../../include/types.h"
#include "../../include/serial.h"
#include "../../include/timer.h"
#include "../../include/vmm.h"
#include "../../include/rtc.h"
#include "../libc/sys/mman.h"

// V8 Headers
#include "v8config.h"
#include <utility>
#include "v8-platform.h"

#include <memory>
#include <vector>

namespace v8 {
namespace platform {
namespace silver {

class SilverPageAllocator : public v8::PageAllocator {
public:
    SilverPageAllocator() = default;
    virtual ~SilverPageAllocator() = default;

    size_t AllocatePageSize() override { return 4096; }
    size_t CommitPageSize() override { return 4096; }

    void SetRandomMmapSeed(int64_t seed) override { (void)seed; }
    void* GetRandomMmapAddr() override { return nullptr; }

    void* AllocatePages(void* address, size_t length, size_t alignment,
                        v8::PageAllocator::Permission permissions) override {
        int prot = PROT_NONE;
        if (permissions == v8::PageAllocator::kRead) prot = PROT_READ;
        else if (permissions == v8::PageAllocator::kReadWrite) prot = PROT_READ | PROT_WRITE;
        else if (permissions == v8::PageAllocator::kReadExecute) prot = PROT_READ | PROT_EXEC;
        else if (permissions == v8::PageAllocator::kReadWriteExecute) prot = PROT_READ | PROT_WRITE | PROT_EXEC;
        
        return vmm_mmap(address, length, prot, MAP_PRIVATE | MAP_ANONYMOUS);
    }

    bool FreePages(void* address, size_t length) override {
        return munmap(address, length) == 0;
    }

    bool ReleasePages(void* address, size_t length, size_t new_length) override {
        (void)address; (void)length; (void)new_length;
        return true; 
    }

    bool SetPermissions(void* address, size_t length, v8::PageAllocator::Permission permissions) override {
        int prot = PROT_NONE;
        if (permissions == v8::PageAllocator::kRead) prot = PROT_READ;
        else if (permissions == v8::PageAllocator::kReadWrite) prot = PROT_READ | PROT_WRITE;
        else if (permissions == v8::PageAllocator::kReadExecute) prot = PROT_READ | PROT_EXEC;
        else if (permissions == v8::PageAllocator::kReadWriteExecute) prot = PROT_READ | PROT_WRITE | PROT_EXEC;

        return vmm_mprotect(address, length, prot) == 0;
    }

    bool DecommitPages(void* address, size_t size) override {
        return vmm_mprotect(address, size, PROT_NONE) == 0;
    }
};

class SilverTaskRunner : public v8::TaskRunner {
public:
    bool IdleTasksEnabled() override { return false; }
    
    void PostTaskImpl(std::unique_ptr<v8::Task> task,
                      const v8::SourceLocation& location) override {
        (void)location;
        task->Run();
    }

    void PostDelayedTaskImpl(std::unique_ptr<v8::Task> task,
                             double delay_in_seconds,
                             const v8::SourceLocation& location) override {
        (void)delay_in_seconds; (void)location;
        task->Run();
    }
};

class SilverPlatform : public v8::Platform {
public:
    SilverPlatform() : page_allocator_(std::make_unique<SilverPageAllocator>()) {}
    
    PageAllocator* GetPageAllocator() override { return page_allocator_.get(); }

    int NumberOfWorkerThreads() override { return 0; }

    std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) override {
        (void)isolate; (void)priority;
        if (!foreground_task_runner_) {
            foreground_task_runner_ = std::make_shared<SilverTaskRunner>();
        }
        return foreground_task_runner_;
    }

    void PostTaskOnWorkerThreadImpl(v8::TaskPriority priority,
                                    std::unique_ptr<v8::Task> task,
                                    const v8::SourceLocation& location) override {
        (void)priority; (void)location;
        task->Run();
    }

    void PostDelayedTaskOnWorkerThreadImpl(v8::TaskPriority priority,
                                           std::unique_ptr<v8::Task> task,
                                           double delay_in_seconds,
                                           const v8::SourceLocation& location) override {
        (void)priority; (void)delay_in_seconds; (void)location;
        task->Run();
    }

    double MonotonicallyIncreasingTime() override {
        return (double)timer_get_ms() / 1000.0;
    }

    double CurrentClockTimeMillis() override {
        return (double)timer_get_ms(); 
    }

    v8::TracingController* GetTracingController() override {
        static v8::TracingController controller;
        return &controller;
    }

    std::unique_ptr<v8::JobHandle> CreateJobImpl(
        v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task,
        const v8::SourceLocation& location) override {
        (void)priority; (void)job_task; (void)location;
        return std::unique_ptr<v8::JobHandle>();
    }
private:
    std::unique_ptr<SilverPageAllocator> page_allocator_;
    std::shared_ptr<v8::TaskRunner> foreground_task_runner_; 
};

} // namespace silver
} // namespace platform
} // namespace v8

extern "C" v8::Platform* create_silver_platform() {
    return new v8::platform::silver::SilverPlatform();
}
