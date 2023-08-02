#include <iostream>
#include "../kdmapper/intel_driver.hpp"

using namespace intel_driver;

int main()
{
    HANDLE hDriver = Load();
    int x = 100;
    int y = 0;
    ReadMemory(hDriver, (uint64_t)& x, &y, sizeof(int));
    uint64_t p = AllocatePool(hDriver, nt::POOL_TYPE::NonPagedPool, 10);
    std::cout << "分配内存：" << std::hex << p << "\n";
    FreePool(hDriver, p);
    p = GetKernelModuleExport(hDriver, ntoskrnlAddr, "ExAllocatePool");
    std::cout << "函数地址：" << std::hex << p << "\n";
    Unload(hDriver);
    std::cout << "读取内存" << y << "\n";
    return 0;
}
