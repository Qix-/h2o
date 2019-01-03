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
#ifndef H2O_LUA_H__
#define H2O_LUA_H__
#pragma once

#include <lua.h>
#include <lauxlib.h>
#include "h2o.h"

typedef struct st_h2o_lua_handler_t {
    h2o_handler_t super; /* must be first */

    const char *config_filename;
    int config_line;

    h2o_pathconf_t *pathconf;

    lua_State *L;
    int chunk_ref;
} h2o_lua_handler_t;

/* handler/configurator/lua.c */
void h2o_lua_register_configurator(h2o_globalconf_t *conf);

/* handler/lua.c */
int h2o_lua_register_handler(
    h2o_pathconf_t *pathconf,
    const char *config_filename, int config_line,
    const char *script_src, const char *script_name);

#endif
