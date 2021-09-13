// Based on https://github.com/georgebrock/readline-example/blob/master/main.c

#define _GNU_SOURCE

#define RETERR  -1
#define RETSXS   0
#define CMDLEN   10
#define MAX_ARGV 20

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <readline/readline.h>

#include "json.h"

typedef struct st_cmdopts {
	int   pos;
	char *type;
	char **opts;
} basecmd_t;
basecmd_t basecmd = {0};

char *default_cmds[MAX_ARGV] = {
	"auth", "reauth", "su", "get",
	"goto", "put", "xplore", "uname",
	 NULL
};

char **resolve_command(const char *, int, int);
char *get_matched_command(const char *, int);
char *escape(const char *);
int quote_detector(char *, int);
int print_basecmd_buff(void);
int free_basecmd(void);
void handle_basecmd_type(basecmd_t basecmd);

char *gcmdref;
char **gcmdlist = default_cmds;
bool reset_default_cmd = false;
bool enable_file_completion = false;

static
void scan_cmdopts(const char *str, int len, void *data)
{
	int i;
	struct json_token t;
	basecmd_t *basecmd = (basecmd_t *)data;

	basecmd->opts = malloc(MAX_ARGV * sizeof(*basecmd->opts));
	
	for (i = 0; json_scanf_array_elem(str, len, "", i, &t) > 0; i++) {
		if (i == MAX_ARGV) {
			puts("MAX argv");
			abort();
		}

		basecmd->opts[i] = strndup(t.ptr, t.len);
		//printf("argv[%d] = '%s'\n", i, basecmd->opts[i]);
	}

	return;
}

char *get_matched_command(const char *text, int state)
{
	char *retval = NULL;
	char *name = NULL;
	static int list_index, len;

	do {
		if (reset_default_cmd == true) {
			//printf("%s: resetting default_cmds ...\n", __func__);
			enable_file_completion = false;
			free_basecmd();
			gcmdlist = default_cmds;
		}

		if (!state) {
			list_index = 0;
			len = strlen(text);
		}

		while ((name = gcmdlist[list_index++])) {
			if (rl_completion_quote_character) {
				name = strdup(name);
			} else {
				name = escape(name);
			}

			if (strncmp(name, text, len) == 0) {
				retval = name;
				break;
			} else {
				free(name);
			}
		}

	} while(0);

	//printf("%s: text='%s' state=%d name='%s'\n", __func__, text, state, name);

	return retval;
}

char **resolve_command(const char *text, int start, int end)
{
	int i = 0;
	static int pos = 0;
	char *field = NULL;
	char *tmpbase = NULL;
	int n = 0, nitems = 0;
	char **cmdlist = NULL;
	struct  json_token jsontoken = {0};
	char **retval = NULL;

	do {
		//printf("\n%s: text=\"%s\", start=%d, end=%d\n", __func__, text, start, end);

		if (enable_file_completion == true) {
			rl_attempted_completion_over = 0;
		} else { /* disable_file_completion */
			rl_attempted_completion_over = 1;
		}

		if ((basecmd.opts != NULL) && (start < basecmd.pos)) {
			reset_default_cmd = true;
		}

		cmdlist = rl_completion_matches(text, get_matched_command);
		retval = cmdlist;
		if (cmdlist == NULL) {
			break; /* return */
		}

		/* calculate no.of items in cmdlist */
		for(n = 0; cmdlist[n] != NULL; n++);
		//print_cmdlist("cmdlist", cmdlist);

		/* if list contain only one item,
		   that is the expected one    */
		/* FIXME: Implement linked list to old multiple basecmds
		   This will fix memleak "auth auth auth" */
		if (n == 1) {
			reset_default_cmd = false;

			/* process sub-commands */
			tmpbase = cmdlist[n - 1];
			if (tmpbase == NULL) {
				break; /*return */
			}

			pos = (start + strlen(tmpbase) + 1);
			basecmd.pos = pos;

			/* get resolved command options from json file */
			asprintf(&field, ".%s", tmpbase);
			nitems = json_scanf_array_elem(gcmdref, strlen(gcmdref), field, i, &jsontoken);
			if (nitems > 0) {
				if (jsontoken.type != JSON_TYPE_OBJECT_END) {
					printf("%s:%d %s jscanf incomplete !!!\n", __FILE__, __LINE__, __func__);
					abort();
				}

				nitems = json_scanf(jsontoken.ptr, jsontoken.len, "{ type: %Q, opts: %M}", &basecmd.type, scan_cmdopts, &basecmd);
				if (nitems <= 0) {
					printf("%s: failed to scan field '%s' in json data", __func__, field);
					abort();
				}

				if (nitems <= 0) {
					printf("%s: failed to scan field '%s' in json data", __func__, field);
					abort();
				}
			}

			if (basecmd.opts != NULL) {
				gcmdlist = basecmd.opts;
			}

			handle_basecmd_type(basecmd);
			free(field);
		} /* if (n == 1) */
	} while(0);

	return retval;
}

/* This function will call before readline 
   prints the prompt in readline() call */
int readline_startup_callback(void)
{
	free_basecmd();
	gcmdlist = default_cmds;
	rl_attempted_completion_over = 1;

	return 0;
}

int main(int argc, char *argv[])
{
	char *buffer = NULL;

	rl_char_is_quoted_p = &quote_detector;
	rl_completer_quote_characters = "'\"";
	rl_completer_word_break_characters = " ";
	rl_attempted_completion_function = resolve_command;
	rl_startup_hook = readline_startup_callback;

	gcmdref = json_fread("commands.json");
	if (gcmdref == NULL) {
		printf("json_fread failed to read %s\n", "commands.json");
		return -1;
	}

	while(1) {
		buffer = readline("kshell $ ");
		if (buffer) {
			if (strlen(buffer) > 0) {
				printf("%s\n", buffer);
			}
			free(buffer);
		}
	}

	free(gcmdref);

	return 0;
}

void handle_basecmd_type(basecmd_t basecmd)
{
	if (strcmp(basecmd.type, "FCTL") == 0) {
		//puts("enable file completion");
		enable_file_completion = true;
	} else {
		//puts("disabled file completion");
		enable_file_completion = false;
	}

	return;
}

int free_basecmd(void)
{
	int i = 0;

	basecmd.pos = 0;

	if (basecmd.type != NULL) {
		free(basecmd.type); basecmd.type = NULL;
	}

	if (basecmd.opts != NULL) {
		for(i = 0; basecmd.opts[i] != NULL; i++) {
			free(basecmd.opts[i]); basecmd.opts[i] = NULL;
		}

		free(basecmd.opts); basecmd.opts = NULL;
	}

	return 0;
}

int print_basecmd_buff(void)
{
	int i = 0;

	printf("basecmd.type: %s\n"
	       "basecmd.pos : %d\n"
	       "basecmd.opts:", basecmd.type, basecmd.pos);

	if (basecmd.opts != NULL) {
		for(i = 0; basecmd.opts[i] != NULL; i++) {
			fprintf(stdout, "  %s  ", basecmd.opts[i]);
		}
	}

	puts("");

	return 0;
}

int print_cmdlist(char *msg, char *cmdlist[])
{
	int n = 0;

	if (cmdlist == NULL) {
		printf("%s: %s = NULL\n", __func__, msg);
		return -1;
	}

	for(n = 0; cmdlist[n] != NULL; n++) {
		printf("%s: %s[%d] = %s\n", __func__, msg, n, cmdlist[n]);
	}

	return n;
}

char *escape(const char *original)
{
	size_t original_len;
	int i, j;
	char *escaped, *resized_escaped;

	original_len = strlen(original);

	if (original_len > SIZE_MAX / 2) {
		errx(1, "string too long to escape");
	}

	if ((escaped = malloc(2 * original_len + 1)) == NULL) {
		err(1, NULL);
	}

	for (i = 0, j = 0; i < original_len; ++i, ++j) {
		if (original[i] == ' ') {
			escaped[j++] = '\\';
		}
		escaped[j] = original[i];
	}
	escaped[j] = '\0';

	if ((resized_escaped = realloc(escaped, j)) == NULL) {
		free(escaped);
		resized_escaped = NULL;
		err(1, NULL);
	}

	return resized_escaped;
}

int quote_detector(char *line, int index)
{
	return (
			index > 0 &&
			line[index - 1] == '\\' &&
			!quote_detector(line, index - 1)
	       );
}

//EOF
