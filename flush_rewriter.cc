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

#include <regex>
#include <string>

#include "plugin/flush_rewriter/flush_rewriter.h"

bool query_rewritten(const std::string &query, std::string *rewritten_query, bool force_local) {
  /*
    This plugin is intended to parse FLUSH statements,
    which starts with 'FLUSH '; hence, we can check the first
    character to exit as early as possible.
  */
  if (!force_local) return false; 

  if (query.length() == 0 || (query[0] != 'F' && query[0] != 'f')) return false;

  const std::regex flush_cmd("^FLUSH\\s+(?!.*(LOCAL|NO_WRITE_TO_BINLOG))",
                                std::regex::icase | std::regex::nosubs);
  if (!regex_search(query, flush_cmd)) return false;

  /*
    We now know this is a FLUSH statement not LOCAL. So we can go ahead with
    replacements.

    We just need to add LOCAL
  */
  const std::regex local_option(
      "^FLUSH\\s+(?!.*(LOCAL|NO_WRITE_TO_BINLOG))",
      std::regex::icase | std::regex::nosubs);
  *rewritten_query = std::regex_replace(query, local_option, "FLUSH LOCAL ");

  return (*rewritten_query != query);
}
