/**
 * Copyright (c) 2022, Timothy Stack
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
 */

#ifndef lnav_console_hh
#define lnav_console_hh

#include <set>
#include <vector>

#include "base/attr_line.hh"

namespace lnav {
namespace console {

void println(FILE* file, const attr_line_t& al);

struct snippet {
    static snippet from(std::string src, const attr_line_t& content)
    {
        snippet retval;

        retval.s_source = std::move(src);
        retval.s_content = content;
        return retval;
    }

    snippet& with_line(int32_t line)
    {
        this->s_line = line;
        return *this;
    }

    snippet& with_column(int32_t column)
    {
        this->s_column = column;
        return *this;
    }

    std::string s_source;
    int32_t s_line{0};
    int32_t s_column{0};
    attr_line_t s_content;
};

struct user_message {
    enum class level {
        ok,
        info,
        warning,
        error,
    };

    static user_message error(const attr_line_t& al);

    static user_message warning(const attr_line_t& al);

    static user_message info(const attr_line_t& al);

    static user_message ok(const attr_line_t& al);

    user_message& with_reason(const attr_line_t& al)
    {
        this->um_reason = al;
        this->um_reason.rtrim();
        return *this;
    }

    user_message& with_errno_reason()
    {
        this->um_reason = strerror(errno);
        return *this;
    }

    user_message& with_snippet(const snippet& sn)
    {
        this->um_snippets.emplace_back(sn);
        return *this;
    }

    user_message& with_snippets(std::vector<snippet> snippets)
    {
        this->um_snippets.insert(this->um_snippets.end(),
                                 std::make_move_iterator(snippets.begin()),
                                 std::make_move_iterator(snippets.end()));
        return *this;
    }

    user_message& with_note(const attr_line_t& al)
    {
        this->um_notes.emplace_back(al);

        return *this;
    }

    user_message& with_help(const attr_line_t& al)
    {
        this->um_help = al;
        this->um_help.rtrim();

        return *this;
    }

    enum class render_flags {
        prefix,
    };

    attr_line_t to_attr_line(std::set<render_flags> flags
                             = {render_flags::prefix}) const;

    level um_level{level::ok};
    attr_line_t um_message;
    std::vector<snippet> um_snippets;
    attr_line_t um_reason;
    std::vector<attr_line_t> um_notes;
    attr_line_t um_help;
};

void print(FILE* file, const user_message& um);

}  // namespace console
}  // namespace lnav

#endif
