/*
 * Copyright (c) 2019 DeNA Co., Ltd., Josh Junon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "h2o.h"
#include "h2o/lua_.h"
#include "h2o/memory.h"

const char * const string_wrapper[2] = {
    "return function(req)\n",
    "\nend\n"
};

/* be very selective with which libraries you choose to load. */
/* also worth mentioning: coroutines are included by default
   in luajit - there is no luaopen_x() for them. */
static const luaL_Reg loadedlibs[] = {
    {"_G", luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_TABLIBNAME, luaopen_table},
    /*{LUA_IOLIBNAME, luaopen_io},*/
    /*{LUA_OSLIBNAME, luaopen_os},*/
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_BITLIBNAME, luaopen_bit},
    /*{LUA_JITLIBNAME, luaopen_jit},*/
    /*{LUA_FFILIBNAME, luaopen_ffi},*/
    /*{LUA_DBLIBNAME, luaopen_debug},*/
    {NULL, NULL}
};

struct read_state_t {
    int state;
    h2o_iovec_t *source;
};

static void lua_open_h2o_libs(lua_State *L) {
    const luaL_Reg *lib = loadedlibs;
    for (; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

static int on_req(h2o_handler_t *_handler, h2o_req_t *req)
{
    h2o_lua_handler_t *handler = (void *)_handler;
    h2o_lua_context_t *ctx = h2o_context_get_handler_context(req->conn->ctx, &handler->super);

    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->chunk_ref);

    /* wrap the request object */
    h2o_lua__push_request(ctx->L, req);

    switch (lua_pcall(ctx->L, 1, 0, 0)) {
        case 0:
            break;
        case LUA_ERRRUN:
            h2o_req_log_error(req, "lua", " lua: error: %s (%.*s:%d)\n",
                lua_tostring(ctx->L, -1),
                (int) handler->config_filename.len, handler->config_filename.base, handler->config_line);
            lua_pop(ctx->L, 1);
            goto handler_failure;
        case LUA_ERRMEM:
            h2o_req_log_error(req, "lua", " lua: memory allocation failed while running script (%.*s:%d)\n",
                (int) handler->config_filename.len, handler->config_filename.base, handler->config_line);
            goto handler_failure;
        case LUA_ERRERR:
            h2o_req_log_error(req, "lua", " lua: there was an error from within the error handler during script execution: %s (%.*s:%d)\n",
                lua_tostring(ctx->L, -1),
                (int) handler->config_filename.len, handler->config_filename.base, handler->config_line);
            lua_pop(ctx->L, 1);
            goto handler_failure;
        default:
            h2o_req_log_error(req, "lua", " lua: unknown error while running script (%.*s:%d)\n",
                (int) handler->config_filename.len, handler->config_filename.base, handler->config_line);
            goto handler_failure;
    }

    req->res.status = 200; /* XXX */
    h2o_send_inline(req, H2O_STRLIT("")); /* XXX */
    return 0;

handler_failure:
    h2o_send_error_500(req, "Internal Server Error", "Internal Server Error", 0);
    return 0;
}

static const char * lua_string_reader(lua_State *L, void *data, size_t *size)
{
    struct read_state_t *state = data;

    int stage_no = state->state;
    ++state->state;

    switch (stage_no) {
    case 0:
        *size = strlen(string_wrapper[0]);
        return string_wrapper[0];
    case 1:
        *size = state->source->len;
        return state->source->base;
    case 2:
        *size = strlen(string_wrapper[1]);
        return string_wrapper[1];
    case 3:
        *size = 0;
        return NULL;
    }

    assert(0 && "invalid lua string read state");
    *size = 0;
    return NULL; /* to make the compiler happy in release builds */
}

static void on_context_init(h2o_handler_t *_handler, h2o_context_t *ctx)
{
    h2o_lua_handler_t *handler = (void *)_handler;
    h2o_lua_context_t *handler_ctx = h2o_mem_alloc(sizeof(*handler_ctx));

    handler_ctx->handler = handler;

    errno = 0;
    handler_ctx->L = luaL_newstate();

    if (handler_ctx->L == NULL) {
        fprintf(stderr, "lua: could not allocate new lua state: %s\n", strerror(errno));
        exit(1);
    }

    lua_open_h2o_libs(handler_ctx->L);

    h2o_lua__mt_register_request(handler_ctx->L);

    struct read_state_t read_state;
    read_state.state = 0;
    read_state.source = &handler->script_source;

    switch (lua_load(handler_ctx->L, &lua_string_reader, &read_state, handler->script_name.base)) {
        case 0:
            break;
        case LUA_ERRSYNTAX:
            fprintf(stderr, "lua: syntax error: %s (%s:%d)\n",
                lua_tostring(handler_ctx->L, -1),
                handler->config_filename.base, handler->config_line);
            lua_pop(handler_ctx->L, 1);
            goto load_failure;
        case LUA_ERRMEM:
            fprintf(stderr, "lua: memory allocation failed while loading script (%s:%d)\n",
                handler->config_filename.base, handler->config_line);
            goto load_failure;
        default:
            fprintf(stderr, "lua: unknown error while loading script (%s:%d)\n",
                handler->config_filename.base, handler->config_line);
            goto load_failure;
    }

    switch (lua_pcall(handler_ctx->L, 0, 1, 0)) {
        case 0:
            break;
        case LUA_ERRRUN:
            fprintf(stderr, "lua: error: %s (%s:%d)\n",
                lua_tostring(handler_ctx->L, -1),
                handler->config_filename.base, handler->config_line);
            lua_pop(handler_ctx->L, 1);
            goto load_failure;
        case LUA_ERRMEM:
            fprintf(stderr, "lua: memory allocation failed while running script (%s:%d)\n",
                handler->config_filename.base, handler->config_line);
            goto load_failure;
        case LUA_ERRERR:
            fprintf(stderr, "lua: there was an error from within the error handler during script execution: %s (%s:%d)\n",
                lua_tostring(handler_ctx->L, -1),
                handler->config_filename.base, handler->config_line);
            lua_pop(handler_ctx->L, 1);
            goto load_failure;
        default:
            fprintf(stderr, "lua: unknown error while running script (%s:%d)\n",
                handler->config_filename.base, handler->config_line);
            goto load_failure;
    }

    /* did the script return a function? */
    if (!lua_isfunction(handler_ctx->L, -1)) {
        fprintf(stderr, "lua: handler scripts must return a function object (got %s) (%s:%d)\n",
            lua_typename(handler_ctx->L, lua_type(handler_ctx->L, -1)),
                handler->config_filename.base, handler->config_line);
            lua_pop(handler_ctx->L, 1);
            goto load_failure;
    }

    handler_ctx->chunk_ref = luaL_ref(handler_ctx->L, LUA_REGISTRYINDEX);
    assert(handler_ctx->chunk_ref >= 0); /* since -1 means error */

    /* success! */
    h2o_context_set_handler_context(ctx, &handler->super, handler_ctx);
    return;

load_failure:
    lua_close(handler_ctx->L);
    exit(1);
}

static void on_context_dispose(h2o_handler_t *_handler, h2o_context_t *ctx)
{
    h2o_lua_handler_t *handler = (void *)_handler;
    h2o_lua_context_t *handler_ctx = h2o_context_get_handler_context(ctx, &handler->super);

    if (handler_ctx == NULL) {
        return;
    }

    if (handler_ctx->L != NULL) {
        lua_close(handler_ctx->L);
    }

    free(handler_ctx);
}

static void on_handler_dispose(h2o_handler_t *_handler)
{
    h2o_lua_handler_t *handler = (void *)_handler;

    free(handler->config_filename.base);
    free(handler->script_name.base);
    free(handler->script_source.base);
    free(handler);
}

int h2o_lua_register_handler(
    h2o_pathconf_t *pathconf,
    const char *config_filename, int config_line,
    const char *script_src, const char *script_name)
{
    h2o_lua_handler_t *handler = (void *)h2o_create_handler(pathconf, sizeof(*handler));

    handler->super.on_context_init = on_context_init;
    handler->super.on_context_dispose = on_context_dispose;
    handler->super.dispose = on_handler_dispose;
    handler->super.on_req = on_req;

    handler->config_filename = h2o_strdup(NULL, config_filename, strlen(config_filename));
    handler->config_line = config_line;
    handler->pathconf = pathconf;
    handler->script_name = h2o_strdup(NULL, script_name, strlen(script_name));
    handler->script_source = h2o_strdup(NULL, script_src, strlen(script_src));

    return 0;
}
