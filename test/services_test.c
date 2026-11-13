#include "check.h"
#include "weivel/services.h"

int main(void) {
    check_text(WEIVEL_STANDARD_SERVICE_CONFIG, "weivel.config", "config macro");
    check_text(WEIVEL_STANDARD_SERVICE_SECRETS, "weivel.secrets", "secrets macro");
    check_text(WEIVEL_STANDARD_SERVICE_PATHS, "weivel.paths", "paths macro");
    check_text(weivel_standard_service_config(), "weivel.config", "config function");
    check_text(weivel_standard_service_secrets(), "weivel.secrets", "secrets function");
    check_text(weivel_standard_service_paths(), "weivel.paths", "paths function");
    return check_summary("services_test");
}
