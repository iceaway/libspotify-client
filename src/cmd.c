#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include <unistd.h>
#include "cmd.h"
#include "session.h"
#include "common.h"

#define MAX_ARGUMENTS		32
#define BUFFER_SIZE		512
#define MAX_USERNAME_LENGTH	128
#define	MAX_PASSWORD_LENGTH	128

struct command {
	char *command;
	char *help;
	int (*callback_fn)(int argc, char *argv[]);
};

static int cmd_quit(int argc, char *argv[]);
static int cmd_help(int argc, char *argv[]);
static int cmd_echo(int argc, char *argv[]);
static int cmd_login(int argc, char *argv[]);
static int cmd_logout(int argc, char *argv[]);
static int cmd_logging(int argc, char *argv[]);
static int cmd_state(int argc, char *argv[]);
static int cmd_search(int argc, char *argv[]);

void cmd_done(void);

const struct command cmd_table[] = {
	{ "quit", "Quit the console", cmd_quit },
	{ "help", "Print a list of commands", cmd_help },
	{ "echo", "Echo to terminal window", cmd_echo },
	{ "login", "Login to spotify", cmd_login },
	{ "logout", "Logout from spotify", cmd_logout },
	{ "log", "Set logging on/off", cmd_logging },
	{ "state", "Display connection state", cmd_state },
	{ "search", "Search for songs", cmd_search }
};
const unsigned int cmd_table_length = sizeof(cmd_table) / sizeof(cmd_table[0]);
static pthread_t g_cmdline_thread;

static int g_show_prompt;
static char *g_cmdline = NULL;
static pthread_mutex_t g_cmdline_mutex;
static pthread_cond_t g_cmdline_cond;
static int g_end_program = 0;
static notify_callback_fn g_ncb;

 int cmd_search(int argc, char *argv[])
{
	if (argc <= 1) {
		fprintf(stderr, "Not enough arguments. usage: %s \"search pattern\"\n", argv[0]);
		return -1;
	} else {
		session_search(argv[1]);
	}
	return 0;

}

static int cmd_state(int argc, char *argv[])
{
	int state = session_state();
	(void)argc;
	(void)argv;

	printf("session state is: %d\n", state);
	if (state >= 0)
		return 0;
	else
		return -1;
}

static int cmd_logging(int argc, char *argv[])
{
	if (argc == 1) {
		printf("logging is %s\n", session_get_log_state() == 0 ? "disabled" : "enabled");
	} else if (argc > 1) {
		if (strcmp(argv[1], "on") == 0) {
			session_set_log_state(1);
			printf("enabling logging\n");
		} else if (strcmp(argv[1], "off") == 0) {
			session_set_log_state(0);
			printf("disabling logging\n");
		} else {
			printf("invalid argument, should be enable or disable\n");
			return -1;
		}
	} else {
		return -1;
	}
	return 0;


}

static int cmd_login(int argc, char *argv[])
{
	int ret;
	char username[MAX_USERNAME_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	(void)argc;
	(void)argv;

	printf("Username: ");
	fgets(username, MAX_USERNAME_LENGTH, stdin);
	printf("Password: ");
	fgets(password, MAX_PASSWORD_LENGTH, stdin);
	
	username[strlen(username)-1] = '\0';
	password[strlen(password)-1] = '\0';

	if (password != NULL) {
		ret = session_login(username, password);
	} else {
		fprintf(stderr, "failed to read password\n");
		ret = -1;
	}

	memset(password, 0, sizeof(password));
	return ret;
}

static int cmd_logout(int argc, char *argv[])
{
	int ret;
	(void)argc;
	(void)argv;
	ret = session_logout();
	return ret;
}

static int cmd_echo(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; ++i)
		printf("%s ", argv[i]);
	printf("\n");
	return 0;
}

static int cmd_help(int argc, char *argv[])
{
	unsigned int i;
	(void)argc;
	(void)argv;
	printf("These are the available commands\n");
	for (i = 0; i < cmd_table_length; ++i)
		printf("\t%s\t\t%s\n", cmd_table[i].command, cmd_table[i].help);

	return 0;
}
static int cmd_quit(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	session_set_program_finished();
	g_end_program = 1;
	return 0;
}

static unsigned int find_cmd(char *cmd)
{
	unsigned int i = -1;
	char *p;
	for (i = 0; i < cmd_table_length; ++i) {
		p = cmd_table[i].command;
		if (strcmp(cmd, p) == 0)
			return i;
	}

	return -1;	       	
}


static int parse_cmdline(char *data, size_t len)
{
	char *buf = malloc(len + 1);
	char *p;
	char *argv[MAX_ARGUMENTS];
	int argc = 0;
	int cur_argv = 0;
	int i = 0;
	int copy = 0;
	int within_quotes = 0;
	int ret;

	p = &data[0];

	/* First argument is the command itself */
	argv[cur_argv++] = &buf[0];

	/* Remove leading white space */
	while (*p == ' ')
		++p;

	while (*p) {
		if (*p == '\\') {
			p++;
			copy = 1;
		} else if (*p == '"') {
			within_quotes = !within_quotes;
			copy = 0;
		} else if ((p < &data[len]) && (*p == ' ') && (*(p+1) == ' ') && !within_quotes) {
			copy = 0; /* Ignore white spaces */
		} else if ((*p == ' ') && !within_quotes) {
			buf[i++] = '\0';
			argv[cur_argv] = &buf[i];
			++cur_argv;
			copy = 0;
		} else if (*p == '\n') {
			buf[i++] = '\0';
			copy = 0;
		} else {
			copy = 1;
		}
		if (copy)
			buf[i++] = *p;
		++p;
	}
	buf[i] = '\0';

	if (within_quotes) {
		fprintf(stderr, "Error, no closing quotes!\n");
		free(buf);
		return -1;
	}
	
	argv[cur_argv] = NULL;
	argc = cur_argv;
	ret = find_cmd(argv[0]);
	if (ret == -1) {
		fprintf(stderr, "Unknown command %s\n", argv[0]);
		free(buf);
		return -2;
	} else {
		if (cmd_table[ret].callback_fn != NULL) {
			int r = cmd_table[ret].callback_fn(argc, argv);
			fprintf(stderr, "%s returned %d\n", cmd_table[ret].command, r);
		}
	}

	free(buf);
	return 0;

}

void cmd_done(void)
{
	pthread_mutex_lock(&g_cmdline_mutex);
	g_show_prompt = 1;
	pthread_cond_signal(&g_cmdline_cond);
	pthread_mutex_unlock(&g_cmdline_mutex);
}

static void *cmdline_prompt(void *arg)
{
	(void)arg;

	printf("Welcome. type 'help' for a list of commands or press CTRL-C to exit\n");
	while (1) {
		pthread_mutex_lock(&g_cmdline_mutex);
		while (!g_show_prompt)
			pthread_cond_wait(&g_cmdline_cond, &g_cmdline_mutex);
		if (g_end_program) {
			break;
		}
		g_cmdline = readline(">> ");
		g_show_prompt = 0;
		pthread_mutex_unlock(&g_cmdline_mutex);
		g_ncb();
	}
	pthread_exit(NULL);
}

int cmd_init(notify_callback_fn cb)
{
	if (cb == NULL)
		return -1;

	g_ncb = cb;
	g_show_prompt = 1;

	pthread_cond_init(&g_cmdline_cond, NULL);
	pthread_mutex_init(&g_cmdline_mutex, NULL);

	if (pthread_create(&g_cmdline_thread, NULL, &cmdline_prompt, NULL) != 0) {
		fprintf(stderr, "failed to create cmdline thread\n");
		return -2;
	}

	return 0;

}

int cmd_destroy(void)
{
	pthread_join(g_cmdline_thread, NULL);
	pthread_cond_destroy(&g_cmdline_cond);
	pthread_mutex_destroy(&g_cmdline_mutex);
	return 0;
}

void cmd_process(void)
{
	if (strlen(g_cmdline) > 0)
		parse_cmdline(g_cmdline, strlen(g_cmdline));
	cmd_done();
}



