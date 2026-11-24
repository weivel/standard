#ifndef WEIVEL_STANDARD_DESCRIPTIONS_H
#define WEIVEL_STANDARD_DESCRIPTIONS_H

/**
 * @file
 * The descriptions of the standard's definitions that other definitions nest, as `weivel/description.h` defines a
 * description, generated from the standard's headers in its own build. A library describing a definition that carries
 * one nests it by its symbol, naming the definition as described elsewhere, and never describes it again. Each answers
 * a static, read-only description that lives until the library is unloaded, and names no answering function, since it
 * is only ever nested.
 *
 * A library serving one of the standard's services describes it itself: a description names the one function that
 * answers its definition, so one shared by every library serving the service could name none of them. It runs
 * weivel_describe over the standard's header for that definition, with ANSWERED_BY its own function and
 * DESCRIBED_ELSEWHERE for the definitions it nests. The bytes are generated from the same header by `offsetof` and
 * `sizeof`, so the copy cannot drift.
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

#endif
