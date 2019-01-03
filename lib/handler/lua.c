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

static void on_handler_dispose(h2o_handler_t *_handler) {
    h2o_lua_handler_t *handler = (void *)_handler;

    if (handler->L != NULL) {
        lua_close(handler->L);
    }

    h2o_mem_free(handler);
}

static int on_req(h2o_handler_t *_handler, h2o_req_t *req) {
    fprintf(stderr, "lua: DEBUG req!\n");
    return 0;
}

static const char * lua_string_reader(lua_State *L, void *data, size_t *size)
{
    const char **script = data;

    if (*script == NULL) {
        *size = 0;
        return NULL;
    }

    const char *result = *script;
    *size = strlen(result);
    *script = NULL;

    return result;
}

int h2o_lua_register_handler(
    h2o_pathconf_t *pathconf,
    const char *config_filename, int config_line,
    const char *script_src, const char *script_name)
{
    h2o_lua_handler_t *handler = (void *)h2o_create_handler(pathconf, sizeof(*handler));

    handler->super.on_context_init = NULL;
    handler->super.on_context_dispose = NULL;
    handler->super.dispose = on_handler_dispose;
    handler->super.on_req = on_req;

    handler->config_filename = config_filename;
    handler->config_line = config_line;
    handler->pathconf = pathconf;
    handler->chunk_ref = -1;

    errno = 0;
    handler->L = luaL_newstate();

    if (handler->L == NULL) {
        fprintf(stderr, "lua: could not allocate new lua state: %s\n", strerror(errno));
        return -1;
    }

    const char *script = script_src;

    switch (lua_load(handler->L, &lua_string_reader, &script, script_name)) {
        case 0:
            goto load_success;
        case LUA_ERRSYNTAX:
            fprintf(stderr, "lua: syntax error: %s (%s:%d)\n",
                lua_tostring(handler->L, -1),
                config_filename, config_line);
            lua_pop(handler->L, 1);
            break;
        case LUA_ERRMEM:
            fprintf(stderr, "lua: memory allocation failed while loading script (%s:%d)\n",
                config_filename, config_line);
            break;
        default:
            fprintf(stderr, "lua: unknown error while loading script (%s:%d)\n",
                config_filename, config_line);
            break;
    }

    /* failure */
    lua_close(handler->L);
    handler->L = NULL;
    return -1;

load_success:
    handler->chunk_ref = luaL_ref(handler->L, LUA_REGISTRYINDEX);
    assert(handler->chunk_ref >= 0); /* since -1 means error */
    return 0;
}
