#include <dlfcn.h>
#include <stdio.h>
#include <ffnvcodec/nvEncodeAPI.h>

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    void *cuda = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    void *nvenc = dlopen("libnvidia-encode.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!cuda || !nvenc) {
        fprintf(stderr, "Driver load failed: %s\n", dlerror());
        return 1;
    }
    int (*initialize)(unsigned) = (int (*)(unsigned))dlsym(cuda, "cuInit");
    int (*create)(void **, unsigned, int) =
        (int (*)(void **, unsigned, int))dlsym(cuda, "cuCtxCreate_v2");
    int (*destroy)(void *) = (int (*)(void *))dlsym(cuda, "cuCtxDestroy_v2");
    NVENCSTATUS (*create_api)(NV_ENCODE_API_FUNCTION_LIST *) =
        (NVENCSTATUS (*)(NV_ENCODE_API_FUNCTION_LIST *))dlsym(nvenc, "NvEncodeAPICreateInstance");
    if (!initialize || !create || !destroy || !create_api) {
        fprintf(stderr, "Driver entry point missing: %s\n", dlerror());
        return 1;
    }
    int result = initialize(0);
    void *context = NULL;
    if (result == 0)
        result = create(&context, 0, 0);
    if (result != 0) {
        fprintf(stderr, "CUDA initialization/context failed: %d\n", result);
        return 1;
    }
    NV_ENCODE_API_FUNCTION_LIST api = {0};
    api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS status = create_api(&api);
    void *encoder = NULL;
    if (status == NV_ENC_SUCCESS) {
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS parameters = {0};
        parameters.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        parameters.device = context;
        parameters.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
        parameters.apiVersion = NVENCAPI_VERSION;
        status = api.nvEncOpenEncodeSessionEx(&parameters, &encoder);
        printf("nvEncOpenEncodeSessionEx: %d\n", status);
    } else {
        fprintf(stderr, "NvEncodeAPICreateInstance: %d\n", status);
    }
    if (encoder) {
        NVENCSTATUS closed = api.nvEncDestroyEncoder(encoder);
        printf("nvEncDestroyEncoder: %d\n", closed);
        if (closed != NV_ENC_SUCCESS)
            status = closed;
    }
    result = destroy(context);
    printf("cuCtxDestroy_v2: %d\n", result);
    dlclose(nvenc);
    dlclose(cuda);
    return status == NV_ENC_SUCCESS && result == 0 ? 0 : 1;
}
