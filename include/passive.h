#include <netdb.h>
#include "selector.h"

/** handler del socket pasivo que atiende conexiones socksv5 */
void socks_passive_accept(struct selector_key *key);

/** libera pools internos */
void sock_pool_destroy(void);
