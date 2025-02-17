/**
 * Copyright (c) 2007-2012, Timothy Stack
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
 * @file bookmarks.cc
 */

#include "bookmarks.hh"

#include "config.h"

std::unordered_set<std::string> bookmark_metadata::KNOWN_TAGS;

void
bookmark_metadata::add_tag(const std::string& tag)
{
    if (std::find(this->bm_tags.begin(), this->bm_tags.end(), tag)
        == this->bm_tags.end())
    {
        this->bm_tags.push_back(tag);
    }
}

bool
bookmark_metadata::remove_tag(const std::string& tag)
{
    auto iter = std::find(this->bm_tags.begin(), this->bm_tags.end(), tag);
    bool retval = false;

    if (iter != this->bm_tags.end()) {
        this->bm_tags.erase(iter);
        retval = true;
    }
    return retval;
}

bool
bookmark_metadata::empty() const
{
    return this->bm_name.empty() && this->bm_comment.empty()
        && this->bm_tags.empty();
}

void
bookmark_metadata::clear()
{
    this->bm_comment.clear();
    this->bm_tags.clear();
}

bookmark_type_t*
bookmark_type_t::find_type(const std::string& name)
{
    auto iter = std::find_if(type_begin(), type_end(), mark_eq(name));
    bookmark_type_t* retval = nullptr;

    if (iter != type_end()) {
        retval = (*iter);
    }
    return retval;
}
