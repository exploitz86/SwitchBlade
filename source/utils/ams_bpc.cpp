#include <switch.h>

extern "C" {

static Service g_amsBpcSrv;
static bool g_amsBpcInitialized = false;

Result amsBpcInitialize() {
    if (g_amsBpcInitialized) {
        return 0;
    }

    Handle h;
    Result rc = svcConnectToNamedPort(&h, "bpc:ams");
    if (R_SUCCEEDED(rc)) {
        serviceCreate(&g_amsBpcSrv, h);
        g_amsBpcInitialized = true;
    }
    return rc;
}

void amsBpcExit() {
    if (g_amsBpcInitialized) {
        serviceClose(&g_amsBpcSrv);
        g_amsBpcInitialized = false;
    }
}

Result amsBpcSetRebootPayload(const void* src, size_t src_size) {
    if (!g_amsBpcInitialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    return serviceDispatch(&g_amsBpcSrv, 65001,
        .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
        .buffers = { { src, src_size } },
    );
}

}
