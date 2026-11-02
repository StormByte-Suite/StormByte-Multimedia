#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argv;
    void *library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    int (*initialize)(unsigned) = (int (*)(unsigned))dlsym(library, "cuInit");
    int (*device_count)(int *) = (int (*)(int *))dlsym(library, "cuDeviceGetCount");
    int (*error_name)(int, const char **) =
        (int (*)(int, const char **))dlsym(library, "cuGetErrorName");
    if (!initialize || !device_count || !error_name) {
        fprintf(stderr, "CUDA driver entry point missing: %s\n", dlerror());
        dlclose(library);
        return 1;
    }
    int result = initialize(0);
    const char *name = NULL;
    error_name(result, &name);
    printf("cuInit: %d (%s)\n", result, name ? name : "unknown");
    if (result == 0) {
        int count = 0;
        result = device_count(&count);
        printf("cuDeviceGetCount: %d; devices=%d\n", result, count);
    }
    if (result == 0 && argc > 1) {
        int (*create)(void **, unsigned, int) =
            (int (*)(void **, unsigned, int))dlsym(library, "cuCtxCreate_v2");
        int (*destroy)(void *) = (int (*)(void *))dlsym(library, "cuCtxDestroy_v2");
        if (!create || !destroy) {
            fprintf(stderr, "CUDA context entry point missing: %s\n", dlerror());
            dlclose(library);
            return 1;
        }
        void *context = NULL;
        result = create(&context, 0, 0);
        printf("cuCtxCreate_v2: %d\n", result);
        if (result == 0) {
            result = destroy(context);
            printf("cuCtxDestroy_v2: %d\n", result);
        }
    }
    dlclose(library);
    return result == 0 ? 0 : 1;
}
