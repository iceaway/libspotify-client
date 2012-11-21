#ifndef SESSION_H_
#define SESSION_H_

int session_logout(void);
int session_init(void);
void session_set_log_state(int state);
int session_search(char *pattern);
int session_state(void);
int session_get_log_state(void);
int session_release(void);
int session_is_logged_in(void);
void session_set_program_finished(void);
int session_login(char *username, char *password);

#endif
