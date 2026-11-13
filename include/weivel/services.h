#ifndef WEIVEL_STANDARD_SERVICES_H
#define WEIVEL_STANDARD_SERVICES_H

#include "weivel/standard_api.h"

/** The service id of the settings a host reads. */
#define WEIVEL_STANDARD_SERVICE_CONFIG "weivel.config"
/** The service id of the secrets a host keeps. */
#define WEIVEL_STANDARD_SERVICE_SECRETS "weivel.secrets"
/** The service id of the places a host stores files. */
#define WEIVEL_STANDARD_SERVICE_PATHS "weivel.paths"

/** The same ids as functions, for a binding that cannot read a macro; each answers a static NUL-terminated string. */
WEIVEL_STANDARD_API const char *weivel_standard_service_config(void);
WEIVEL_STANDARD_API const char *weivel_standard_service_secrets(void);
WEIVEL_STANDARD_API const char *weivel_standard_service_paths(void);

#endif
