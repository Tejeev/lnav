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
 */

#include <vector>

#include "command_executor.hh"

#include "base/ansi_scrubber.hh"
#include "base/fs_util.hh"
#include "base/injector.hh"
#include "base/string_util.hh"
#include "bound_tags.hh"
#include "config.h"
#include "db_sub_source.hh"
#include "help_text_formatter.hh"
#include "lnav.hh"
#include "lnav_config.hh"
#include "lnav_util.hh"
#include "log_format_loader.hh"
#include "papertrail_proc.hh"
#include "service_tags.hh"
#include "shlex.hh"
#include "sql_util.hh"
#include "yajlpp/json_ptr.hh"

exec_context INIT_EXEC_CONTEXT;

static const std::string MSG_FORMAT_STMT = R"(
SELECT count(*) AS total, min(log_line) AS log_line, log_msg_format
    FROM all_logs
    GROUP BY log_msg_format
    ORDER BY total DESC
)";

int
sql_progress(const struct log_cursor& lc)
{
    static sig_atomic_t sql_counter = 0;

    size_t total = lnav_data.ld_log_source.text_line_count();
    off_t off = lc.lc_curr_line;

    if (off < 0) {
        return 0;
    }

    if (lnav_data.ld_window == nullptr) {
        return 0;
    }

    if (!lnav_data.ld_looping) {
        return 1;
    }

    if (ui_periodic_timer::singleton().time_to_update(sql_counter)) {
        lnav_data.ld_bottom_source.update_loading(off, total);
        lnav_data.ld_top_source.update_time();
        lnav_data.ld_status[LNS_TOP].do_update();
        lnav_data.ld_status[LNS_BOTTOM].do_update();
        refresh();
    }

    return 0;
}

void
sql_progress_finished()
{
    if (lnav_data.ld_window == nullptr) {
        return;
    }

    lnav_data.ld_bottom_source.update_loading(0, 0);
    lnav_data.ld_top_source.update_time();
    lnav_data.ld_status[LNS_TOP].do_update();
    lnav_data.ld_status[LNS_BOTTOM].do_update();
    lnav_data.ld_views[LNV_DB].redo_search();
}

Result<std::string, lnav::console::user_message> execute_from_file(
    exec_context& ec,
    const ghc::filesystem::path& path,
    int line_number,
    char mode,
    const std::string& cmdline);

Result<std::string, lnav::console::user_message>
execute_command(exec_context& ec, const std::string& cmdline)
{
    std::vector<std::string> args;

    log_info("Executing: %s", cmdline.c_str());

    split_ws(cmdline, args);

    if (!args.empty()) {
        readline_context::command_map_t::iterator iter;

        if ((iter = lnav_commands.find(args[0])) == lnav_commands.end()) {
            return ec.make_error("unknown command - {}", args[0]);
        }

        ec.ec_current_help = &iter->second->c_help;
        auto retval = iter->second->c_func(ec, cmdline, args);
        ec.ec_current_help = nullptr;
        return retval;
    }

    return ec.make_error("no command to execute");
}

Result<std::string, lnav::console::user_message>
execute_sql(exec_context& ec, const std::string& sql, std::string& alt_msg)
{
    db_label_source& dls = lnav_data.ld_db_row_source;
    auto_mem<sqlite3_stmt> stmt(sqlite3_finalize);
    struct timeval start_tv, end_tv;
    std::string stmt_str = trim(sql);
    std::string retval;
    int retcode;

    log_info("Executing SQL: %s", sql.c_str());

    lnav_data.ld_bottom_source.grep_error("");

    if (startswith(stmt_str, ".")) {
        std::vector<std::string> args;
        split_ws(stmt_str, args);

        auto sql_cmd_map = injector::get<readline_context::command_map_t*,
                                         sql_cmd_map_tag>();
        auto cmd_iter = sql_cmd_map->find(args[0]);

        if (cmd_iter != sql_cmd_map->end()) {
            ec.ec_current_help = &cmd_iter->second->c_help;
            auto retval = cmd_iter->second->c_func(ec, stmt_str, args);
            ec.ec_current_help = nullptr;

            return retval;
        }
    }

    if (stmt_str == ".msgformats") {
        stmt_str = MSG_FORMAT_STMT;
    }

    ec.ec_accumulator->clear();

    auto source = ec.ec_source.top();
    sql_progress_guard progress_guard(sql_progress,
                                      sql_progress_finished,
                                      source.s_source,
                                      source.s_line,
                                      source.s_content);
    gettimeofday(&start_tv, nullptr);
    retcode = sqlite3_prepare_v2(
        lnav_data.ld_db.in(), stmt_str.c_str(), -1, stmt.out(), nullptr);
    if (retcode != SQLITE_OK) {
        const char* errmsg = sqlite3_errmsg(lnav_data.ld_db);

        alt_msg = "";
        return ec.make_error("{}", errmsg);
    }
    if (stmt == nullptr) {
        alt_msg = "";
        return ec.make_error("No statement given");
    }
#ifdef HAVE_SQLITE3_STMT_READONLY
    else if (ec.is_read_only() && !sqlite3_stmt_readonly(stmt.in()))
    {
        return ec.make_error(
            "modifying statements are not allowed in this context: {}", sql);
    }
#endif
    else
    {
        bool done = false;
        int param_count;

        param_count = sqlite3_bind_parameter_count(stmt.in());
        for (int lpc = 0; lpc < param_count; lpc++) {
            std::map<std::string, std::string>::iterator ov_iter;
            const char* name;

            name = sqlite3_bind_parameter_name(stmt.in(), lpc + 1);
            ov_iter = ec.ec_override.find(name);
            if (ov_iter != ec.ec_override.end()) {
                sqlite3_bind_text(stmt.in(),
                                  lpc,
                                  ov_iter->second.c_str(),
                                  ov_iter->second.length(),
                                  SQLITE_TRANSIENT);
            } else if (name[0] == '$') {
                const auto& lvars = ec.ec_local_vars.top();
                const auto& gvars = ec.ec_global_vars;
                std::map<std::string, std::string>::const_iterator local_var,
                    global_var;
                const char* env_value;

                if (lnav_data.ld_window) {
                    char buf[32];
                    int lines, cols;

                    getmaxyx(lnav_data.ld_window, lines, cols);
                    if (strcmp(name, "$LINES") == 0) {
                        snprintf(buf, sizeof(buf), "%d", lines);
                        sqlite3_bind_text(
                            stmt.in(), lpc + 1, buf, -1, SQLITE_TRANSIENT);
                    } else if (strcmp(name, "$COLS") == 0) {
                        snprintf(buf, sizeof(buf), "%d", cols);
                        sqlite3_bind_text(
                            stmt.in(), lpc + 1, buf, -1, SQLITE_TRANSIENT);
                    }
                }

                if ((local_var = lvars.find(&name[1])) != lvars.end()) {
                    sqlite3_bind_text(stmt.in(),
                                      lpc + 1,
                                      local_var->second.c_str(),
                                      -1,
                                      SQLITE_TRANSIENT);
                } else if ((global_var = gvars.find(&name[1])) != gvars.end()) {
                    sqlite3_bind_text(stmt.in(),
                                      lpc + 1,
                                      global_var->second.c_str(),
                                      -1,
                                      SQLITE_TRANSIENT);
                } else if ((env_value = getenv(&name[1])) != nullptr) {
                    sqlite3_bind_text(
                        stmt.in(), lpc + 1, env_value, -1, SQLITE_STATIC);
                }
            } else if (name[0] == ':' && ec.ec_line_values != nullptr) {
                for (auto& lv : *ec.ec_line_values) {
                    if (lv.lv_meta.lvm_name != &name[1]) {
                        continue;
                    }
                    switch (lv.lv_meta.lvm_kind) {
                        case value_kind_t::VALUE_BOOLEAN:
                            sqlite3_bind_int64(
                                stmt.in(), lpc + 1, lv.lv_value.i);
                            break;
                        case value_kind_t::VALUE_FLOAT:
                            sqlite3_bind_double(
                                stmt.in(), lpc + 1, lv.lv_value.d);
                            break;
                        case value_kind_t::VALUE_INTEGER:
                            sqlite3_bind_int64(
                                stmt.in(), lpc + 1, lv.lv_value.i);
                            break;
                        case value_kind_t::VALUE_NULL:
                            sqlite3_bind_null(stmt.in(), lpc + 1);
                            break;
                        default:
                            sqlite3_bind_text(stmt.in(),
                                              lpc + 1,
                                              lv.text_value(),
                                              lv.text_length(),
                                              SQLITE_TRANSIENT);
                            break;
                    }
                }
            } else {
                sqlite3_bind_null(stmt.in(), lpc + 1);
                log_warning("Could not bind variable: %s", name);
            }
        }

        if (lnav_data.ld_rl_view != nullptr) {
            lnav_data.ld_rl_view->set_value("Executing query: " + sql + " ...");
        }

        ec.ec_sql_callback(ec, stmt.in());
        while (!done) {
            retcode = sqlite3_step(stmt.in());

            switch (retcode) {
                case SQLITE_OK:
                case SQLITE_DONE:
                    done = true;
                    break;

                case SQLITE_ROW:
                    ec.ec_sql_callback(ec, stmt.in());
                    break;

                default: {
                    const char* errmsg;

                    log_error("sqlite3_step error code: %d", retcode);
                    errmsg = sqlite3_errmsg(lnav_data.ld_db);
                    if (startswith(errmsg, "lnav-error:")) {
                        return Err(lnav::from_json<lnav::console::user_message>(
                            &errmsg[11]));
                    }
                    return ec.make_error("{}", errmsg);
                }
            }
        }

        if (!dls.dls_rows.empty() && !ec.ec_local_vars.empty()
            && !ec.ec_dry_run) {
            auto& vars = ec.ec_local_vars.top();

            for (unsigned int lpc = 0; lpc < dls.dls_headers.size(); lpc++) {
                const auto& column_name = dls.dls_headers[lpc].hm_name;

                if (sql_ident_needs_quote(column_name.c_str())) {
                    continue;
                }

                const auto* value = dls.dls_rows[0][lpc];
                if (value == nullptr) {
                    continue;
                }

                vars[column_name] = value;
            }
        }

        if (lnav_data.ld_rl_view != nullptr) {
            lnav_data.ld_rl_view->set_value("");
        }
    }

    gettimeofday(&end_tv, nullptr);
    if (retcode == SQLITE_DONE) {
        if (lnav_data.ld_log_source.is_line_meta_changed()) {
            lnav_data.ld_log_source.text_filters_changed();
            lnav_data.ld_views[LNV_LOG].reload_data();
        }
        lnav_data.ld_filter_view.reload_data();
        lnav_data.ld_files_view.reload_data();
        lnav_data.ld_views[LNV_DB].reload_data();
        lnav_data.ld_views[LNV_DB].set_left(0);

        if (!ec.ec_accumulator->empty()) {
            retval = ec.ec_accumulator->get_string();
        } else if (!dls.dls_rows.empty()) {
            if (lnav_data.ld_flags & LNF_HEADLESS) {
                if (ec.ec_local_vars.size() == 1) {
                    ensure_view(&lnav_data.ld_views[LNV_DB]);
                }

                retval = "";
                alt_msg = "";
            } else if (dls.dls_rows.size() == 1) {
                auto& row = dls.dls_rows[0];

                if (dls.dls_headers.size() == 1) {
                    retval = row[0];
                } else {
                    for (unsigned int lpc = 0; lpc < dls.dls_headers.size();
                         lpc++) {
                        if (lpc > 0) {
                            retval.append("; ");
                        }
                        retval.append(dls.dls_headers[lpc].hm_name);
                        retval.push_back('=');
                        retval.append(row[lpc]);
                    }
                }
            } else {
                int row_count = dls.dls_rows.size();
                char row_count_buf[128];
                struct timeval diff_tv;

                timersub(&end_tv, &start_tv, &diff_tv);
                snprintf(row_count_buf,
                         sizeof(row_count_buf),
                         ANSI_BOLD("%'d") " row%s matched in " ANSI_BOLD(
                             "%ld.%03ld") " seconds",
                         row_count,
                         row_count == 1 ? "" : "s",
                         diff_tv.tv_sec,
                         std::max((long) diff_tv.tv_usec / 1000, 1L));
                retval = row_count_buf;
                alt_msg = HELP_MSG_2(
                    y,
                    Y,
                    "to move forward/backward through query results "
                    "in the log view");
            }
        }
#ifdef HAVE_SQLITE3_STMT_READONLY
        else if (sqlite3_stmt_readonly(stmt.in()))
        {
            retval = "info: No rows matched";
            alt_msg = "";

            if (lnav_data.ld_flags & LNF_HEADLESS) {
                if (ec.ec_local_vars.size() == 1) {
                    ensure_view(&lnav_data.ld_views[LNV_DB]);
                }
            }
        }
#endif
    }

    return Ok(retval);
}

static Result<std::string, lnav::console::user_message>
execute_file_contents(exec_context& ec,
                      const ghc::filesystem::path& path,
                      bool multiline)
{
    static ghc::filesystem::path stdin_path("-");
    static ghc::filesystem::path dev_stdin_path("/dev/stdin");

    std::string retval;
    FILE* file;

    if (path == stdin_path || path == dev_stdin_path) {
        if (isatty(STDIN_FILENO)) {
            return ec.make_error("stdin has already been consumed");
        }
        file = stdin;
    } else if ((file = fopen(path.c_str(), "r")) == nullptr) {
        return ec.make_error("unable to open file");
    }

    int line_number = 0, starting_line_number = 0;
    auto_mem<char> line;
    size_t line_max_size;
    ssize_t line_size;
    std::string cmdline;
    char mode = '\0';

    ec.ec_path_stack.emplace_back(path.parent_path());
    exec_context::output_guard og(ec);
    while ((line_size = getline(line.out(), &line_max_size, file)) != -1) {
        line_number += 1;

        if (trim(line.in()).empty()) {
            continue;
        }
        if (line[0] == '#') {
            continue;
        }

        switch (line[0]) {
            case ':':
            case '/':
            case ';':
            case '|':
                if (mode) {
                    retval = TRY(execute_from_file(
                        ec, path, starting_line_number, mode, trim(cmdline)));
                }

                starting_line_number = line_number;
                mode = line[0];
                cmdline = std::string(&line[1]);
                break;
            default:
                if (multiline) {
                    cmdline += line;
                } else {
                    retval = TRY(execute_from_file(
                        ec, path, line_number, ':', line.in()));
                }
                break;
        }
    }

    if (mode) {
        retval = TRY(execute_from_file(
            ec, path, starting_line_number, mode, trim(cmdline)));
    }

    if (file == stdin) {
        if (isatty(STDOUT_FILENO)) {
            log_perror(dup2(STDOUT_FILENO, STDIN_FILENO));
        }
    } else {
        fclose(file);
    }
    ec.ec_path_stack.pop_back();

    return Ok(retval);
}

Result<std::string, lnav::console::user_message>
execute_file(exec_context& ec, const std::string& path_and_args, bool multiline)
{
    available_scripts scripts;
    std::vector<std::string> split_args;
    std::string retval, msg;
    shlex lexer(path_and_args);

    log_info("Executing file: %s", path_and_args.c_str());

    if (!lexer.split(split_args, ec.ec_local_vars.top())) {
        return ec.make_error("unable to parse path");
    }
    if (split_args.empty()) {
        return ec.make_error("no script specified");
    }

    ec.ec_local_vars.push({});

    auto script_name = split_args[0];
    auto& vars = ec.ec_local_vars.top();
    char env_arg_name[32];
    std::string star, open_error = "file not found";

    add_ansi_vars(vars);

    snprintf(
        env_arg_name, sizeof(env_arg_name), "%d", (int) split_args.size() - 1);

    vars["#"] = env_arg_name;
    for (size_t lpc = 0; lpc < split_args.size(); lpc++) {
        snprintf(env_arg_name, sizeof(env_arg_name), "%lu", lpc);
        vars[env_arg_name] = split_args[lpc];
    }
    for (size_t lpc = 1; lpc < split_args.size(); lpc++) {
        if (lpc > 1) {
            star.append(" ");
        }
        star.append(split_args[lpc]);
    }
    vars["__all__"] = star;

    std::vector<script_metadata> paths_to_exec;

    find_format_scripts(lnav_data.ld_config_paths, scripts);
    auto iter = scripts.as_scripts.find(script_name);
    if (iter != scripts.as_scripts.end()) {
        paths_to_exec = iter->second;
    }
    if (script_name == "-" || script_name == "/dev/stdin") {
        paths_to_exec.push_back({script_name, "", "", ""});
    } else if (access(script_name.c_str(), R_OK) == 0) {
        struct script_metadata meta;

        meta.sm_path = script_name;
        extract_metadata_from_file(meta);
        paths_to_exec.push_back(meta);
    } else if (errno != ENOENT) {
        open_error = strerror(errno);
    } else {
        auto script_path = ghc::filesystem::path(script_name);

        if (!script_path.is_absolute()) {
            script_path = ec.ec_path_stack.back() / script_path;
        }

        if (ghc::filesystem::is_regular_file(script_path)) {
            struct script_metadata meta;

            meta.sm_path = script_path;
            extract_metadata_from_file(meta);
            paths_to_exec.push_back(meta);
        } else if (errno != ENOENT) {
            open_error = strerror(errno);
        }
    }

    if (!paths_to_exec.empty()) {
        for (auto& path_iter : paths_to_exec) {
            retval
                = TRY(execute_file_contents(ec, path_iter.sm_path, multiline));
        }
    }
    ec.ec_local_vars.pop();

    if (paths_to_exec.empty()) {
        return ec.make_error(
            "unknown script -- {} -- {}", script_name, open_error);
    }

    return Ok(retval);
}

Result<std::string, lnav::console::user_message>
execute_from_file(exec_context& ec,
                  const ghc::filesystem::path& path,
                  int line_number,
                  char mode,
                  const std::string& cmdline)
{
    std::string retval, alt_msg;
    auto _sg = ec.enter_source(path.string(), line_number, cmdline);

    switch (mode) {
        case ':':
            retval = TRY(execute_command(ec, cmdline));
            break;
        case '/':
            lnav_data.ld_view_stack.top() |
                [cmdline](auto tc) { tc->execute_search(cmdline.substr(1)); };
            break;
        case ';':
            setup_logline_table(ec);
            retval = TRY(execute_sql(ec, cmdline, alt_msg));
            break;
        case '|':
            retval = TRY(execute_file(ec, cmdline));
            break;
        default:
            retval = TRY(execute_command(ec, cmdline));
            break;
    }

    log_info("%s:%d:execute result -- %s",
             path.c_str(),
             line_number,
             retval.c_str());

    return Ok(retval);
}

Result<std::string, lnav::console::user_message>
execute_any(exec_context& ec, const std::string& cmdline_with_mode)
{
    std::string retval, alt_msg, cmdline = cmdline_with_mode.substr(1);
    auto _cleanup = finally([&ec] {
        if (ec.is_read_write() &&
            // only rebuild in a script or non-interactive mode so we don't
            // block the UI.
            (lnav_data.ld_flags & LNF_HEADLESS || ec.ec_path_stack.size() > 1))
        {
            rescan_files();
            rebuild_indexes_repeatedly();
        }
    });

    switch (cmdline_with_mode[0]) {
        case ':':
            retval = TRY(execute_command(ec, cmdline));
            break;
        case '/':
            lnav_data.ld_view_stack.top() |
                [cmdline](auto tc) { tc->execute_search(cmdline.substr(1)); };
            break;
        case ';':
            setup_logline_table(ec);
            retval = TRY(execute_sql(ec, cmdline, alt_msg));
            break;
        case '|': {
            retval = TRY(execute_file(ec, cmdline));
            break;
        }
        default:
            retval = TRY(execute_command(ec, cmdline));
            break;
    }

    return Ok(retval);
}

void
execute_init_commands(
    exec_context& ec,
    std::vector<std::pair<Result<std::string, lnav::console::user_message>,
                          std::string>>& msgs)
{
    if (lnav_data.ld_cmd_init_done) {
        return;
    }

    db_label_source& dls = lnav_data.ld_db_row_source;
    int option_index = 1;

    log_info("Executing initial commands");
    for (auto& cmd : lnav_data.ld_commands) {
        std::string alt_msg;

        wait_for_children();

        {
            auto _sg = ec.enter_source("command-option", option_index++, cmd);
            switch (cmd.at(0)) {
                case ':':
                    msgs.emplace_back(execute_command(ec, cmd.substr(1)),
                                      alt_msg);
                    break;
                case '/':
                    lnav_data.ld_view_stack.top() |
                        [cmd](auto tc) { tc->execute_search(cmd.substr(1)); };
                    break;
                case ';':
                    setup_logline_table(ec);
                    msgs.emplace_back(execute_sql(ec, cmd.substr(1), alt_msg),
                                      alt_msg);
                    break;
                case '|':
                    msgs.emplace_back(execute_file(ec, cmd.substr(1)), alt_msg);
                    break;
            }

            rescan_files();
            rebuild_indexes_repeatedly();
        }
    }
    lnav_data.ld_commands.clear();

    if (!lnav_data.ld_pt_search.empty()) {
#ifdef HAVE_LIBCURL
        auto pt = std::make_shared<papertrail_proc>(
            lnav_data.ld_pt_search.substr(3),
            lnav_data.ld_pt_min_time,
            lnav_data.ld_pt_max_time);
        lnav_data.ld_active_files.fc_file_names[lnav_data.ld_pt_search].with_fd(
            pt->copy_fd());
        isc::to<curl_looper&, services::curl_streamer_t>().send(
            [pt](auto& clooper) { clooper.add_request(pt); });
#endif
    }

    if (dls.dls_rows.size() > 1) {
        ensure_view(&lnav_data.ld_views[LNV_DB]);
    }

    lnav_data.ld_cmd_init_done = true;
}

int
sql_callback(exec_context& ec, sqlite3_stmt* stmt)
{
    auto& dls = lnav_data.ld_db_row_source;

    if (!sqlite3_stmt_busy(stmt)) {
        dls.clear();

        return 0;
    }

    stacked_bar_chart<std::string>& chart = dls.dls_chart;
    view_colors& vc = view_colors::singleton();
    int ncols = sqlite3_column_count(stmt);
    int row_number;
    int lpc, retval = 0;

    row_number = dls.dls_rows.size();
    dls.dls_rows.resize(row_number + 1);
    if (dls.dls_headers.empty()) {
        for (lpc = 0; lpc < ncols; lpc++) {
            int type = sqlite3_column_type(stmt, lpc);
            std::string colname = sqlite3_column_name(stmt, lpc);
            bool graphable;

            graphable = ((type == SQLITE_INTEGER || type == SQLITE_FLOAT)
                         && !binary_search(lnav_data.ld_db_key_names.begin(),
                                           lnav_data.ld_db_key_names.end(),
                                           colname));

            dls.push_header(colname, type, graphable);
            if (graphable) {
                int attrs = vc.attrs_for_ident(colname);
                chart.with_attrs_for_ident(colname, attrs);
            }
        }
    }
    for (lpc = 0; lpc < ncols; lpc++) {
        const char* value = (const char*) sqlite3_column_text(stmt, lpc);
        db_label_source::header_meta& hm = dls.dls_headers[lpc];

        dls.push_column(value);
        if ((hm.hm_column_type == SQLITE_TEXT
             || hm.hm_column_type == SQLITE_NULL)
            && hm.hm_sub_type == 0)
        {
            sqlite3_value* raw_value = sqlite3_column_value(stmt, lpc);

            switch (sqlite3_value_type(raw_value)) {
                case SQLITE_TEXT:
                    hm.hm_column_type = SQLITE_TEXT;
                    hm.hm_sub_type = sqlite3_value_subtype(raw_value);
                    break;
            }
        }
    }

    return retval;
}

std::future<std::string>
pipe_callback(exec_context& ec, const std::string& cmdline, auto_fd& fd)
{
    auto out = ec.get_output();

    if (out) {
        FILE* file = *out;

        return std::async(std::launch::async, [&fd, file]() {
            char buffer[1024];
            ssize_t rc;

            if (file == stdout) {
                lnav_data.ld_stdout_used = true;
            }

            while ((rc = read(fd, buffer, sizeof(buffer))) > 0) {
                fwrite(buffer, rc, 1, file);
            }

            return std::string();
        });
    } else {
        auto tmp_fd
            = lnav::filesystem::open_temp_file(
                  ghc::filesystem::temp_directory_path() / "lnav.out.XXXXXX")
                  .map([](auto pair) {
                      ghc::filesystem::remove(pair.first);

                      return std::move(pair.second);
                  })
                  .expect("Cannot create temporary file for callback");
        auto pp = std::make_shared<piper_proc>(
            std::move(fd), false, std::move(tmp_fd));
        static int exec_count = 0;

        lnav_data.ld_pipers.push_back(pp);
        auto desc = fmt::format(
            FMT_STRING("[{}] Output of {}"), exec_count++, cmdline);
        lnav_data.ld_active_files.fc_file_names[desc]
            .with_fd(pp->get_fd())
            .with_include_in_session(false)
            .with_detect_format(false);
        lnav_data.ld_files_to_front.emplace_back(desc, 0);
        if (lnav_data.ld_rl_view != nullptr) {
            lnav_data.ld_rl_view->set_alt_value(
                HELP_MSG_1(X, "to close the file"));
        }

        return lnav::futures::make_ready_future(std::string());
    }
}

void
add_global_vars(exec_context& ec)
{
    for (const auto& iter : lnav_config.lc_global_vars) {
        shlex subber(iter.second);
        std::string str;

        if (!subber.eval(str, ec.ec_global_vars)) {
            log_error("Unable to evaluate global variable value: %s",
                      iter.second.c_str());
            continue;
        }

        ec.ec_global_vars[iter.first] = str;
    }
}

void
exec_context::set_output(const std::string& name,
                         FILE* file,
                         int (*closer)(FILE*))
{
    log_info("redirecting command output to: %s", name.c_str());
    this->ec_output_stack.back().second | [](auto out) {
        if (out.second != nullptr) {
            out.second(out.first);
        }
    };
    this->ec_output_stack.back()
        = std::make_pair(name, std::make_pair(file, closer));
}

void
exec_context::clear_output()
{
    log_info("redirecting command output to screen");
    this->ec_output_stack.back().second | [](auto out) {
        if (out.second != nullptr) {
            out.second(out.first);
        }
    };
    this->ec_output_stack.back() = std::make_pair("default", nonstd::nullopt);
}

exec_context::exec_context(std::vector<logline_value>* line_values,
                           sql_callback_t sql_callback,
                           pipe_callback_t pipe_callback)
    : ec_line_values(line_values),
      ec_accumulator(std::make_unique<attr_line_t>()),
      ec_sql_callback(sql_callback), ec_pipe_callback(pipe_callback)
{
    this->ec_local_vars.push(std::map<std::string, std::string>());
    this->ec_path_stack.emplace_back(".");
    this->ec_source.emplace(
        lnav::console::snippet::from("command", "").with_line(1));
    this->ec_output_stack.emplace_back("screen", nonstd::nullopt);
}

void
exec_context::add_error_context(lnav::console::user_message& um)
{
    if (!this->ec_source.empty()) {
        auto source = this->ec_source.top();

        um.with_snippet(source);
    }

    if (this->ec_current_help != nullptr) {
        attr_line_t help;

        format_help_text_for_term(*this->ec_current_help, -1, help, true);
        um.with_help(help);
    }
}

exec_context::output_guard::output_guard(exec_context& context,
                                         std::string name,
                                         const nonstd::optional<output_t>& file)
    : sg_context(context)
{
    if (file) {
        log_info("redirecting command output to: %s", name.c_str());
    }
    context.ec_output_stack.emplace_back(std::move(name), file);
}

exec_context::output_guard::~output_guard()
{
    this->sg_context.clear_output();
    this->sg_context.ec_output_stack.pop_back();
}
