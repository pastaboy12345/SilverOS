extern "C" {
#include "../../include/serial.h"
#include "../../include/heap.h"
}
#include <stddef.h>

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* p) noexcept {
    kfree(p);
}

void operator delete[](void* p) noexcept {
    kfree(p);
}

void operator delete(void* p, size_t size) noexcept {
    (void)size;
    kfree(p);
}

void operator delete[](void* p, size_t size) noexcept {
    (void)size;
    kfree(p);
}


class V8RuntimeTest {
public:
    V8RuntimeTest() {
        serial_printf("[V8] Global constructor called successfully!\n");
    }
    
    void hello() {
        serial_printf("[V8] Hello from C++ runtime!\n");
    }
};

V8RuntimeTest global_tester;

extern "C" void v8_runtime_test() {
    global_tester.hello();
    
    serial_printf("[V8] Testing dynamic allocation...\n");
    V8RuntimeTest* dynamic_tester = new V8RuntimeTest();
    dynamic_tester->hello();
    delete dynamic_tester;
    serial_printf("[V8] Dynamic allocation test passed!\n");
}

