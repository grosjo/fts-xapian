#ifndef FTS_XAPIAN_PLUGIN_H
#define FTS_XAPIAN_PLUGIN_H

#include "lib.h"
#include "mail-storage-private.h"
#include "mailbox-list-private.h"
#include "mail-search.h"
#include "fts-api.h"
#include "module-context.h"
#include "mail-user.h"
#include "fts-api-private.h"

extern const char *fts_xapian_plugin_dependencies[];
extern struct fts_backend fts_backend_xapian;

void fts_xapian_plugin_init(struct module *module);
void fts_xapian_plugin_deinit(void);

#endif
