#ifndef WEIVEL_STANDARD_DESCRIPTIONS_H
#define WEIVEL_STANDARD_DESCRIPTIONS_H

/**
 * @file
 * The descriptions of the standard's own definitions, as `weivel/description.h` defines a description, generated from
 * the standard's headers in its own build. A library serving one of these services declares the matching symbol as
 * its service's `description`, and a library describing a definition that carries one nests it by its symbol, so
 * neither describes the standard's definitions again. Each answers a static, read-only description that lives until
 * the library is unloaded.
 */

#include "weivel/description.h"
#include "weivel/standard_api.h"

/** weivel_standard_config, `weivel/services.h`. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_config_description(void);
/** weivel_standard_secrets, `weivel/services.h`. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_secrets_description(void);
/** weivel_standard_paths, `weivel/services.h`. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_paths_description(void);
/** weivel_standard_log, `weivel/services.h`. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_log_description(void);
/** weivel_standard_scopes, `weivel/scope.h`, nesting the four above. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_scopes_description(void);
/** weivel_standard_host, `weivel/host.h`. */
WEIVEL_STANDARD_API const weivel_standard_description *weivel_standard_host_description(void);

#endif
