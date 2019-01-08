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
#include "h2o.h"
#include "h2o/lua_.h"

static const char * const req_mt_name = "H2O request";
static const char * const req_global_name = "request";

static const luaL_Reg lua_req_methods[] = {
    {NULL, NULL}
};

static const luaL_Reg lua_req_mt[] = {
    {"__index", h2o_lua__req_mt_index},
    {"__newindex", h2o_lua__req_mt_newindex},
    {"__tostring", h2o_lua__req_mt_tostring},
    {"__gc", h2o_lua__req_mt_gc},
    {NULL, NULL}
};

void h2o_lua__push_request(lua_State *L, h2o_req_t *req)
{
    /* 1 */ lua_pushlightuserdata(L, req);
    /* 2 */ lua_createtable(L, 0, 1); /* per-instance metatable */
    /* 2 */ luaL_register(L, NULL, lua_req_mt);
    /* 3 */ luaL_getmetatable(L, req_mt_name);
    /* 2 */ lua_setmetatable(L, -2);
}

void h2o_lua__register_request(lua_State *L)
{
    /* 1 */ luaL_newmetatable(L, req_mt_name);

    /* 2 */ lua_createtable(L, 0, 0); /* TODO update non-arr number with number of entries */
    /* 2 */ luaL_register(L, NULL, lua_req_methods);
    /* 3 */ lua_pushval(L, -1); /* clone since we need it in two places */

    /* 2 */ lua_pushval(L, -2, "__index");
    /* 1 */ lua_pop(L 1);

    /* 0 */ lua_setglobal(L, -1, req_global_name);
    return;
}

int h2o_lua__req_mt_index(lua_State *L)
{
    lua_
    return 1;
}

int h2o_lua__req_mt_newindex(lua_State *L)
{
    return 0;
}

int h2o_lua__req_mt_gc(lua_State *L)
{
    /* TODO do some sort of request finishing here */
//    /* 0 */ h2o_req_t *req = luaL_checkudata(L, 1, req_mt_name);
//    if (req->status == 0) {
//        if (req->bytes_sent == 0) {
//            req->status = 202;
//        }
//    }
    return 0;
}

static void * get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static char * get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
            break;
        default:
            strncpy(s, "???", maxlen);
            break;
    }

    return s;
}

int h2o_lua__req_mt_tostring(lua_State *L)
{
    /* 0 */ h2o_req_t *req = h2o_lua__checkudata(L, 1, req_mt_name);

    struct sockaddr sa;
    req->conn->callbacks->get_peername(req->conn, &sa);
    char s[INET6_ADDRSTRLEN];

    /* we do snprintf instead of lua_pushfstring() because we can't
       use %lu or %.*s in lua */
    char buf[1024];
    snprintf(buf, sizeof(buf), "request<%lu>(%s) %.*s %.*s",
        req->conn->id,
        get_ip_str(&sa, s, sizeof(s)),
        (int) req->method.len, req->method.base,
        (int) req->path.len, req->path.base);

    /* 1 */ lua_pushstring(L, buf);

    return 1;
}
