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
 * @file curl_looper.hh
 */

#ifndef curl_looper_hh
#define curl_looper_hh

#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/isc.hh"
#include "config.h"

#if !defined(HAVE_LIBCURL)

typedef int CURLcode;

class curl_request {
public:
    curl_request(const std::string& name){};
};

class curl_looper : public isc::service<curl_looper> {
public:
    void start(){};
    void stop(){};
    void add_request(std::shared_ptr<curl_request> cr){};
    void close_request(const std::string& name){};
    void process_all(){};
};

#else
#    include <condition_variable>
#    include <mutex>
#    include <thread>

#    include <curl/curl.h>

#    include "base/auto_mem.hh"
#    include "base/lnav_log.hh"
#    include "base/time_util.hh"

class curl_request {
public:
    curl_request(std::string name)
        : cr_name(std::move(name)), cr_open(true), cr_handle(curl_easy_cleanup),
          cr_completions(0)
    {
        this->cr_handle.reset(curl_easy_init());
        curl_easy_setopt(this->cr_handle, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(
            this->cr_handle, CURLOPT_ERRORBUFFER, this->cr_error_buffer);
        curl_easy_setopt(this->cr_handle, CURLOPT_DEBUGFUNCTION, debug_cb);
        curl_easy_setopt(this->cr_handle, CURLOPT_DEBUGDATA, this);
        curl_easy_setopt(this->cr_handle, CURLOPT_VERBOSE, 1);
        if (getenv("SSH_AUTH_SOCK") != nullptr) {
            curl_easy_setopt(this->cr_handle,
                             CURLOPT_SSH_AUTH_TYPES,
#    ifdef CURLSSH_AUTH_AGENT
                             CURLSSH_AUTH_AGENT |
#    endif
                                 CURLSSH_AUTH_PASSWORD);
        }
    };

    virtual ~curl_request() = default;

    const std::string& get_name() const
    {
        return this->cr_name;
    };

    virtual void close()
    {
        this->cr_open = false;
    };

    bool is_open() const
    {
        return this->cr_open;
    };

    CURL* get_handle() const
    {
        return this->cr_handle;
    };

    int get_completions() const
    {
        return this->cr_completions;
    };

    virtual long complete(CURLcode result)
    {
        double total_time = 0, download_size = 0, download_speed = 0;

        this->cr_completions += 1;
        curl_easy_getinfo(this->cr_handle, CURLINFO_TOTAL_TIME, &total_time);
        log_debug("%s: total_time=%f", this->cr_name.c_str(), total_time);
        curl_easy_getinfo(
            this->cr_handle, CURLINFO_SIZE_DOWNLOAD, &download_size);
        log_debug("%s: download_size=%f", this->cr_name.c_str(), download_size);
        curl_easy_getinfo(
            this->cr_handle, CURLINFO_SPEED_DOWNLOAD, &download_speed);
        log_debug(
            "%s: download_speed=%f", this->cr_name.c_str(), download_speed);

        return -1;
    };

protected:
    static int debug_cb(
        CURL* handle, curl_infotype type, char* data, size_t size, void* userp);

    const std::string cr_name;
    bool cr_open;
    auto_mem<CURL> cr_handle;
    char cr_error_buffer[CURL_ERROR_SIZE];
    int cr_completions;
};

class curl_looper : public isc::service<curl_looper> {
public:
    curl_looper() : cl_curl_multi(curl_multi_cleanup)
    {
        this->cl_curl_multi.reset(curl_multi_init());
    };

    void process_all()
    {
        this->check_for_new_requests();

        this->requeue_requests(LONG_MAX);

        while (!this->cl_handle_to_request.empty()) {
            this->perform_io();

            this->check_for_finished_requests();
        }
    };

    void add_request(const std::shared_ptr<curl_request>& cr)
    {
        require(cr != nullptr);

        this->cl_all_requests.emplace_back(cr);
        this->cl_new_requests.emplace_back(cr);
    };

    void close_request(const std::string& name)
    {
        this->cl_close_requests.emplace_back(name);
    };

protected:
    void loop_body() override;

private:
    void perform_io();
    void check_for_new_requests();
    void check_for_finished_requests();
    void requeue_requests(mstime_t up_to_time);
    std::chrono::milliseconds compute_timeout(
        mstime_t current_time) const override;

    auto_mem<CURLM> cl_curl_multi;
    std::vector<std::shared_ptr<curl_request> > cl_all_requests;
    std::vector<std::shared_ptr<curl_request> > cl_new_requests;
    std::vector<std::string> cl_close_requests;
    std::map<CURL*, std::shared_ptr<curl_request> > cl_handle_to_request;
    std::vector<std::pair<mstime_t, std::shared_ptr<curl_request> > >
        cl_poll_queue;
};
#endif

#endif
