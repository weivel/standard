#include "weivel/host.h"
#include "weivel/scope.h"
#include "weivel/services.h"

const char *weivel_standard_service_config(void) {
    return WEIVEL_STANDARD_SERVICE_CONFIG;
}

const char *weivel_standard_service_secrets(void) {
    return WEIVEL_STANDARD_SERVICE_SECRETS;
}

const char *weivel_standard_service_paths(void) {
    return WEIVEL_STANDARD_SERVICE_PATHS;
}

const char *weivel_standard_service_log(void) {
    return WEIVEL_STANDARD_SERVICE_LOG;
}

const char *weivel_standard_service_scope(void) {
    return WEIVEL_STANDARD_SERVICE_SCOPE;
}

const char *weivel_standard_service_host(void) {
    return WEIVEL_STANDARD_SERVICE_HOST;
}
