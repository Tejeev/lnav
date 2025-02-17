/**
 * Copyright (c) 2015, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file log_search_table.hh
 */

#ifndef lnav_log_search_table_hh
#define lnav_log_search_table_hh

#include <string>
#include <vector>

#include "log_vtab_impl.hh"
#include "pcrepp/pcrepp.hh"
#include "shared_buffer.hh"

class log_search_table : public log_vtab_impl {
public:
    static int pattern_options()
    {
        return PCRE_CASELESS;
    }

    log_search_table(pcrepp pattern, intern_string_t table_name);

    void get_columns_int(std::vector<vtab_column>& cols);

    void get_columns(std::vector<vtab_column>& cols) const override
    {
        cols = this->lst_cols;
    }

    void get_foreign_keys(std::vector<std::string>& keys_inout) const override;

    bool next(log_cursor& lc, logfile_sub_source& lss) override;

    void extract(std::shared_ptr<logfile> lf,
                 uint64_t line_number,
                 shared_buffer_ref& line,
                 std::vector<logline_value>& values) override;

    pcrepp lst_regex;
    shared_buffer_ref lst_current_line;
    pcre_context_static<128> lst_match_context;
    std::vector<logline_value_meta> lst_column_metas;
    int64_t lst_instance;
    std::vector<vtab_column> lst_cols;
};

#endif
