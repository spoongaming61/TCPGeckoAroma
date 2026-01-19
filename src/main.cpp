#include "main.h"
#include <malloc.h>
#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <nn/ac.h>
#include "TCPGecko.h"

/**
    Mandatory plugin information.
    If not set correctly, the loader will refuse to use the plugin.
**/
WUPS_PLUGIN_NAME("TCPGecko");
WUPS_PLUGIN_DESCRIPTION("Port of the TCPGecko features over to an aroma plugin.");
WUPS_PLUGIN_VERSION("v0.2");
WUPS_PLUGIN_AUTHOR("Teotia444 (modified ver. by Shadow Doggo)");
WUPS_USE_WUT_DEVOPTAB();                // Use the wut devoptabs
WUPS_USE_STORAGE("tcpgecko"); // Unique id for the storage api

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    auto root = WUPSConfigCategory(rootHandle);

    uint32_t hostIpAddress = 0;
    nn::ac::GetAssignedAddress(&hostIpAddress);

    char ipAddr[50];
    if (hostIpAddress != 0) {
        snprintf (ipAddr,
            50,
            "Console IP: %u.%u.%u.%u",
            (hostIpAddress >> 24) & 0xFF,
            (hostIpAddress >> 16) & 0xFF,
            (hostIpAddress >> 8) & 0xFF,
            (hostIpAddress >> 0) & 0xFF
            );
    } else {
        snprintf(ipAddr, sizeof (ipAddr), "The console is not connected to a network.");
    }

    root.add(WUPSConfigItemStub::Create (ipAddr));
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI::SaveStorage();
}

OSThread* ct;

OSThread* GetMainThread() {
    return ct;
}

static void thread_deallocator(OSThread *thread, void *stack) {
    free(stack);
    free(thread);
}

ON_APPLICATION_START() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = "TCPGecko"};
    WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);

    ct = OSGetCurrentThread();
    stopSocket(false);

    //init memory space for our socket thread
    auto* socket = static_cast<OSThread *>(calloc(1, sizeof(OSThread)));
    constexpr int stack_size = 3 * 1024 * 1024;

    // asks wiiu to open thread on another core
    if (void *stack_addr = static_cast<uint8_t *>(memalign(8, stack_size)) + stack_size;
        !OSCreateThread(socket, Start, 0, nullptr, stack_addr, stack_size,
            0x10, OS_THREAD_ATTRIB_AFFINITY_ANY)) return;
    OSSetThreadDeallocator(socket, thread_deallocator);
    OSDetachThread(socket);
    OSResumeThread(socket);

}

ON_APPLICATION_REQUESTS_EXIT() {
    stopSocket(true);
}
