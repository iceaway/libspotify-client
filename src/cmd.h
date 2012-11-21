#ifndef CMD_H_
#define CMD_H_

typedef void (*notify_callback_fn)(void);

int cmd_init(notify_callback_fn cb);
void cmd_process(void);
int cmd_destroy(void);

#endif /* CMD_H_ */
