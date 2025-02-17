/* Copyright (c) 2019 Joan Moreau <jom@grosjo.net>, see the included COPYING file */

#ifdef FTS_DOVECOT24

#include "lib.h"
#include "settings.h"
#include "settings-parser.h"
#include "fts-xapian-plugin.h"

#undef DEF
#define DEF(type, name) \
        SETTING_DEFINE_STRUCT_##type(XAPIAN_LABEL"_"#name, name, struct fts_xapian_settings)

static const struct setting_define fts_xapian_setting_defines[] = {
        DEF(UINT, verbose),
        DEF(UINT, lowmemory),
        DEF(UINT, partial),
        DEF(UINT, maxthreads),
        SETTING_DEFINE_LIST_END
};

static const struct fts_xapian_settings fts_xapian_default_settings = {
        .verbose = 1,
        .lowmemory = XAPIAN_MIN_RAM,
        .partial = XAPIAN_DEFAULT_PARTIAL,
        .maxthreads = 0,
};

const struct setting_parser_info fts_xapian_setting_parser_info = {
        .name = XAPIAN_LABEL,

        .defines = fts_xapian_setting_defines,
        .defaults = &fts_xapian_default_settings,

        .struct_size = sizeof(struct fts_xapian_settings),
        .pool_offset1 = 1 + offsetof(struct fts_xapian_settings, pool),
};

const char *fts_xapian_settings_version = DOVECOT_ABI_VERSION;

const struct setting_parser_info *fts_xapian_settings_set_infos[] = {
       &fts_xapian_setting_parser_info,
       NULL,
};

#endif

