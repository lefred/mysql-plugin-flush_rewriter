/*  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2.0,
    as published by the Free Software Foundation.

    This program is also distributed with certain software (including
    but not limited to OpenSSL) that is licensed under separate terms,
    as designated in a particular file or component or in included license
    documentation.  The authors of MySQL hereby grant you an additional
    permission to link the program and your derivative works with the
    separately licensed software that they have included with MySQL.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string>

#include <ctype.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <mysql/psi/mysql_memory.h>
#include <mysql/service_mysql_alloc.h>

#include <mysql/components/my_service.h>

#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_thread.h"  // my_thread_handle needed by mysql_memory.h
#include "plugin/flush_rewriter/flush_rewriter.h"
#include "template_utils.h"

/* Define system variable.  */
static bool force_local;

static MYSQL_SYSVAR_BOOL(
		force_local, force_local,
                PLUGIN_VAR_NOCMDARG,
                "Enable or not the LOCAL rewrite for FLUSH statement", nullptr,
                        nullptr, false );

static SYS_VAR *force_local_system_variables[] = {
	MYSQL_SYSVAR(force_local), nullptr};

/* Instrument the memory allocation. */
#ifdef HAVE_PSI_INTERFACE
static PSI_memory_key key_memory_flush_rewriter;

static PSI_memory_info all_rewrite_memory[] = {
    {&key_memory_flush_rewriter, "flush_rewriter", 0, 0, PSI_DOCUMENT_ME}};

static int plugin_init(MYSQL_PLUGIN) {
  const char *category = "rewriter";
  int count = static_cast<int>(array_elements(all_rewrite_memory));
  mysql_memory_register(category, all_rewrite_memory, count);
  return 0; /* success */
}
#else
#define plugin_init nullptr
#define key_memory_flush_rewriter PSI_NOT_INSTRUMENTED
#endif /* HAVE_PSI_INTERFACE */

static int rewrite_flush(MYSQL_THD, mysql_event_class_t event_class,
                       const void *event) {
  /* We can exit early if this is not a pre-parse event. */
  const struct mysql_event_parse *event_parse =
      static_cast<const struct mysql_event_parse *>(event);
  if (event_class != MYSQL_AUDIT_PARSE_CLASS ||
      event_parse->event_subclass != MYSQL_AUDIT_PARSE_PREPARSE)
    return 0;

  /*
    If the query is rewritten, we must allocate space for the
    resulting query string and set the event flag accordingly.
  */
  std::string rewritten_query;
  if (query_rewritten(std::string(event_parse->query.str), &rewritten_query, force_local )) {
    char *rewritten_query_buf = static_cast<char *>(my_malloc(
        key_memory_flush_rewriter, rewritten_query.length() + 1, MYF(0)));
    strcpy(rewritten_query_buf, rewritten_query.c_str());
    event_parse->rewritten_query->str = rewritten_query_buf;
    event_parse->rewritten_query->length = rewritten_query.length();
    *(reinterpret_cast<int *>(event_parse->flags)) |=
        MYSQL_AUDIT_PARSE_REWRITE_PLUGIN_QUERY_REWRITTEN;
  }

  return 0;
}

/* Audit plugin descriptor. */
static struct st_mysql_audit flush_rewriter_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION, /* interface version */
    nullptr,                       /* release_thd()     */
    rewrite_flush,                   /* event_notify()    */
    {
        0,
        0,
        (unsigned long)MYSQL_AUDIT_PARSE_ALL,
    } /* class mask        */
};

/* Plugin descriptor */
mysql_declare_plugin(audit_log){
    MYSQL_AUDIT_PLUGIN,             /* plugin type             */
    &flush_rewriter_descriptor,     /* type specific descriptor*/
    "flush_rewriter",               /* plugin name             */
    "lefred",                       /* author                  */
    "Rewrite of FLUSH statements.", /* description             */
    PLUGIN_LICENSE_GPL,             /* license                 */
    plugin_init,                    /* plugin initializer      */
    nullptr,                        /* plugin check uninstall  */
    nullptr,                        /* plugin deinitializer    */
    0x0100,                         /* version                 */
    nullptr,                        /* status variables        */
    force_local_system_variables,   /* system variables        */
    nullptr,                        /* reserved                */
    0                               /* flags                   */
} mysql_declare_plugin_end;
