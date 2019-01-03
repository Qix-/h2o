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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "h2o.h"
#include "h2o/configurator.h"
#include "h2o/lua_.h"
#include "h2o/memory.h"

struct luajit_config_vars_t {
    int unused;
    const char *config_file;
    int config_line;
};

struct luajit_configurator_t
{
    h2o_configurator_t super;
    struct luajit_config_vars_t *vars;
    struct luajit_config_vars_t _vars_stack[H2O_CONFIGURATOR_NUM_LEVELS + 1];
    lua_State *L;
};

static int on_config_enter(h2o_configurator_t *_self, h2o_configurator_context_t *ctx, yoml_t *node)
{
    struct luajit_configurator_t *self = (void *)_self;
    memcpy(self->vars + 1, self->vars, sizeof(*self->vars));
    ++self->vars;
    return 0;
}

static int on_config_exit(h2o_configurator_t *_self, h2o_configurator_context_t *ctx, yoml_t *node)
{
    struct luajit_configurator_t *self = (void *)_self;

    --self->vars;

    /* release lua only when global configuration exited */
    if (self->L != NULL && ctx->parent == NULL) {
        lua_close(self->L);
        self->L = NULL;
    }

    return 0;
}

void * h2o_lua_allocator(void *udata, void *ptr, size_t osize, size_t nsize)
{
	(void) udata;
	(void) osize;

	if (nsize == 0) {
		h2o_mem_free(ptr);
		return NULL;
	}

	return h2o_mem_realloc(ptr, nsize);
}

static int on_config_luajit_handler(h2o_configurator_command_t *cmd, h2o_configurator_context_t *ctx, yoml_t *node)
{
    struct luajit_configurator_t *self = (void *)cmd->configurator;

    //self->L = lua_newstate(&h2o_lua_allocator, NULL);
    errno = 0;
    self->L = luaL_newstate();

    if (self->L == NULL) {
        fprintf(stderr, "lua: could not allocate new lua state: %s\n", strerror(errno));
        return -1;
    }

    // XXX DEBUG
    fputs("lua: made new state successfully\n", stderr);

    /* set source */
    /* node->data.scalar */
    self->vars->config_file = node->filename;
    self->vars->config_line = (int)node->line + 1;

    // TODO load source

    fprintf(stderr, "lua: loaded immediate script (%s:%d)\n",
        self->vars->config_file, self->vars->config_line);

    return 0;
}

void h2o_lua_register_configurator(h2o_globalconf_t *conf)
{
    struct luajit_configurator_t *c = (void *)h2o_configurator_create(conf, sizeof(*c));

    c->vars = c->_vars_stack;
    c->super.enter = on_config_enter;
    c->super.exit = on_config_exit;
    h2o_configurator_define_command(
        &c->super, "lua.handler",
        H2O_CONFIGURATOR_FLAG_PATH | H2O_CONFIGURATOR_FLAG_DEFERRED | H2O_CONFIGURATOR_FLAG_EXPECT_SCALAR,
        on_config_luajit_handler);
}
