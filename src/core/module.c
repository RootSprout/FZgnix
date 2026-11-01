#include "module.h"

extern const ng_module_t ng_http_static_module;

static const ng_module_t *registered_modules[] = {
    &ng_http_static_module
};

const ng_module_t *const *ng_modules(size_t *count) {
    *count = sizeof(registered_modules) / sizeof(registered_modules[0]);
    return registered_modules;
}

int ng_modules_init(ng_log_t *log) {
    size_t count;
    const ng_module_t *const *modules = ng_modules(&count);
    for (size_t i = 0; i < count; i++) {
        if (modules[i]->init && modules[i]->init(log) != 0) {
            ng_log_write(log, NG_LOG_ERROR, "module '%s' initialization failed", modules[i]->name);
            return -1;
        }
        ng_log_write(log, NG_LOG_DEBUG, "module '%s' initialized", modules[i]->name);
    }
    return 0;
}

int ng_modules_handle_http(const ng_http_request_t *request, ng_http_response_t *response,
                           ng_pool_t *pool, ng_log_t *log) {
    size_t count;
    const ng_module_t *const *modules = ng_modules(&count);
    for (size_t i = 0; i < count; i++) {
        const ng_module_t *module = modules[i];
        if (!module->handle_http) continue;
        int result = module->handle_http(request, response, pool, log);
        if (result != NG_MODULE_DECLINED) return result;
    }
    return NG_MODULE_DECLINED;
}
