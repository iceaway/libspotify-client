#include <libspotify/api.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "session.h"
#include "cmd.h"
#include "common.h"

#define USER_AGENT "libspotify-server"

static sp_session *g_session = NULL;
static pthread_mutex_t g_notify_mutex;
static pthread_cond_t g_notify_cond;
static int g_is_logged_in = 0;
static int g_notify_events = 0;
static int g_log_enabled = 0;
static int g_notify_cmdline = 0;
static int g_end_program = 0;

static int is_program_finished(void)
{
	int ret = 0;
	ret = g_end_program;
	return ret;
}

void session_set_program_finished(void)
{
	g_end_program = 1;
}

static void search_complete(sp_search *result, void *userdata)
{
	sp_track *track;
	sp_artist *artist;
	int i;
	(void)userdata;

	if (sp_search_error(result) == SP_ERROR_OK) {
		for (i = 0; i < sp_search_num_tracks(result); ++i) {
			track = sp_search_track(result, i);
			artist = sp_track_artist(track, 0);
			printf("%d. %s - %s\n",
			       i, sp_track_name(track), sp_artist_name(artist));
		}
		fflush(stdout);

	} else {
		fprintf(stderr, "failed to search: %s",
				sp_error_message(sp_search_error(result)));
	}

	sp_search_release(result);
}

int session_search(char *pattern)
{
	sp_search_create(g_session, pattern, 0, 20, 0, 20, 0, 20, 0, 20, SP_SEARCH_STANDARD, &search_complete, NULL);
	return 0;
}

int session_state(void)
{
	int state;
	state = sp_session_connectionstate(g_session);
	return state;
}

void session_set_log_state(int state)
{
	if (state)
		g_log_enabled = 1;
	else
		g_log_enabled = 0;
}

int session_get_log_state(void)
{
	return g_log_enabled;
}

int session_is_logged_in(void)
{
	return g_is_logged_in;
}

static void logged_in(sp_session *session, sp_error error)
{
	(void)session;
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "error while logging in %s\n",
				sp_error_message(error));
	}
	printf("Logged in!\n");
	fflush(stdout);
}

static void logged_out(sp_session *session)
{
	(void)session;
	printf("Logged out!\n");
	fflush(stdout);
	pthread_mutex_lock(&g_notify_mutex);
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
	g_is_logged_in = 0;
}

static void connection_error(sp_session *session, sp_error error)
{
	(void)session;
	fprintf(stderr, sp_error_message(error));
}

static void log_message(sp_session *session, const char *data)
{
	(void)session;
	if (session_get_log_state())
		fprintf(stderr, data);

}

static void notify_main_thread(sp_session *session)
{
	(void)session;
	pthread_mutex_lock(&g_notify_mutex);
	g_notify_events = 1;
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
}

int session_logout(void)
{
	sp_error error;
	int retval = 0;
	if (session_is_logged_in()) {
		error = sp_session_logout(g_session);
		if (error != SP_ERROR_OK) {
			fprintf(stderr, "failed to log out %s\n",
					sp_error_message(error));
			retval = -1;
		}
	}
	return retval;
}

int session_release(void)
{
	sp_error error;

	error = sp_session_release(g_session);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "failed to release session: %s\n",
				sp_error_message(error));
		return -1;
	}

	return 0;
}

int session_init(void)
{
	sp_session_config config;
	sp_error error;
	sp_session *session;
	sp_session_callbacks callbacks;

	extern const char g_appkey[];
	extern const size_t g_appkey_size;

	memset(&config, 0, sizeof(config));
	memset(&callbacks, 0, sizeof(callbacks));

	debug("logging in\n");

	if (g_session != NULL)
		return -1;

	callbacks.logged_in = &logged_in;
	callbacks.logged_out = &logged_out;
	callbacks.connection_error = &connection_error;
	callbacks.notify_main_thread = &notify_main_thread;
	callbacks.log_message = &log_message;

	config.api_version = SPOTIFY_API_VERSION;
	config.cache_location = "/tmp";
	config.settings_location = "/tmp";
	config.application_key = g_appkey;
	config.application_key_size = g_appkey_size;
	config.user_agent = USER_AGENT;
	config.callbacks = &callbacks;
	debug("attempting to create session\n");
	error = sp_session_create(&config, &session);
	if (error != SP_ERROR_OK) {
		debug("failed to create session\n");
		fprintf(stderr, "failed to create session: %s\n",
				sp_error_message(error));
		return -2;
	}
	debug("session created\n");
	g_session = session;
	return 0;
}

int session_login(char *username, char *password)
{
	sp_error error;

	if (g_session == NULL)
		return -1;

	error = sp_session_login(g_session, username, password, 0, NULL);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "failed to login %s\n",
				sp_error_message(error));
		return -2;
	}

	g_is_logged_in = 1;
	return 0;
}

static void cmd_notify(void)
{
	pthread_mutex_lock(&g_notify_mutex);
	g_notify_cmdline = 1;
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
}

int main(int argc, char *argv[])
{
	int timeout = 0;
	sp_error error;
	int notify_cmdline = 0;
	int notify_events = 0;
	struct timespec ts;
	
	(void)argc;
	(void)argv;

	pthread_mutex_init(&g_notify_mutex, NULL);
	pthread_cond_init(&g_notify_cond, NULL);
	session_init();
	cmd_init(cmd_notify);
	
	do {
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout / 1000;
		ts.tv_nsec += (timeout % 1000) * 1E6;
		if (ts.tv_nsec > 1E9) {
			ts.tv_sec++;
			ts.tv_nsec -= 1E9;
		}

		pthread_mutex_lock(&g_notify_mutex);
		pthread_cond_timedwait(&g_notify_cond, &g_notify_mutex, &ts);
		notify_cmdline = g_notify_cmdline;
		notify_events = g_notify_events;
		g_notify_cmdline = 0;
		g_notify_events = 0;
		pthread_mutex_unlock(&g_notify_mutex);
		if (notify_cmdline) {
			cmd_process();
		}

		if (notify_events) {
			do {
				error = sp_session_process_events(g_session, &timeout);
				if (error != SP_ERROR_OK)
					fprintf(stderr, "error processing events: %s\n",
							sp_error_message(error));
			} while (timeout == 0);
		}

	} while (!is_program_finished());

	session_logout();
	while (session_is_logged_in()) {
       		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 1;

		pthread_mutex_lock(&g_notify_mutex);
		pthread_cond_timedwait(&g_notify_cond, &g_notify_mutex, &ts);
		notify_events = g_notify_events;
		g_notify_events = 0;
		pthread_mutex_unlock(&g_notify_mutex);
		if (notify_events) {
			do {
				error = sp_session_process_events(g_session, &timeout);
				if (error != SP_ERROR_OK)
					fprintf(stderr, "error processing events: %s\n",
							sp_error_message(error));
			} while (timeout == 0);
		}
	}

	session_release();
	cmd_destroy();

	pthread_mutex_destroy(&g_notify_mutex);
	pthread_cond_destroy(&g_notify_cond);

	return 0;

}

