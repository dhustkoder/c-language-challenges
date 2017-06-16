#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <locale.h>
#include <ncurses.h>
#include "utils/io.h"
#include "network.h"


#define CHAT_STACK_SIZE ((int)24)
#define BUFFER_SIZE     ((int)512)


enum ChatCmd {
	CHATCMD_NORMAL,
	CHATCMD_QUIT
};


static const ConnectionInfo* cinfo      = NULL;     // connection information
static char conn_buffer[BUFFER_SIZE]    = { '\0' }; // buffer for incoming msgs
static char* chatstack[CHAT_STACK_SIZE] = { NULL }; // the chat msg stack with unames
static int chatstack_idx                = 0;        // current chat stack index

static char buffer[BUFFER_SIZE]         = { '\0' }; // text box's buffer for user input
static int blen                         = 0;        // text box's current buffer size
static int bidx                         = 0;        // text box's cursor position in the buffer
static int cy, cx;                                  // current cursor position in the window
static int my, mx;                                  // max y and x positions
static int hy, hx;                                  // text box's home y and x (start position)


static void stackMsgPushBack(char* str)
{
	if (chatstack_idx >= CHAT_STACK_SIZE) {
		free(chatstack[0]);
		for (int i = 0; i < (CHAT_STACK_SIZE - 1); ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = CHAT_STACK_SIZE - 1;
	}
	chatstack[chatstack_idx++] = str;
}


static void stackMsg(const char* const uname, const char* const msg)
{
	char* const str = malloc(snprintf(NULL, 0, "%s: %s", uname, msg) + 1);
	sprintf(str, "%s: %s", uname, msg);
	stackMsgPushBack(str);
}


static void stackInfo(const char* const fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char* const str = malloc(vsnprintf(NULL, 0, fmt, args) + 1);

	va_end(args);
	va_start(args, fmt);

	vsprintf(str, fmt, args);
	stackMsgPushBack(str);

	va_end(args);
}


static void freeMsgStack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
}


static void initializeUI(void)
{
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	intrflush(stdscr, FALSE);
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
}


static void terminateUI(void)
{
	endwin();
}


static void printUI(void)
{
	printw("Host: %s (%s). Client: %s (%s).\n"
	       "=================================================\n",
	       cinfo->host_uname, cinfo->host_ip,
	       cinfo->client_uname, cinfo->client_ip);

	int i;
	for (i = 0; i < chatstack_idx; ++i)
		printw("%s\n", chatstack[i]);
	for (; i < CHAT_STACK_SIZE; ++i)
		printw("\n");

	printw("==================================================\n> ");
}


static void clearTextBox(void)
{
	cy = hy;
	cx = hx;
	blen = 0;
	bidx = 0;
	buffer[0] = '\0';
}


static void moveCursorLeft(void)
{
	if (cy > hy || cx > hx) {
		--bidx;
		--cx;
		if (cx < 0) {
			cx = mx - 1;
			--cy;
		}
		move(cy, cx);
	}
}


static void moveCursorRight(void)
{
	if (bidx < blen) {
		++bidx;
		++cx;
		if (cx >= mx) {
			cx = 0;
			++cy;
		}
		move(cy, cx);
	}
}


static void moveCursorHome(void)
{
	if (bidx != 0) {
		bidx = 0;
		cy = hy;
		cx = hx;
		move(cy, cx);
	}
}


static void moveCursorEnd(void)
{
	if (bidx < blen) {
		bidx = blen;
		cx = hx + (blen % mx);
		cy = hy + (blen / mx);
		move(cy, cx);
	}
}



static void refreshUI(void)
{
	clear();
	move(0, 0);
	printUI();
	getyx(stdscr, hy, hx);
	getmaxyx(stdscr, my, mx);
	cy = hy + (bidx / mx);
	cx = hx + (bidx % mx);
	printw(buffer);
	move(cy, cx);
	refresh();
}


static bool updateTextBox(void)
{
	const int c = getch();

	if (c == ERR)
		return false;

	#ifdef DEBUG_
	stackInfo("KEY PRESSED %i", c);
	#endif

	switch (c) {
	case 10: // also enter (ascii) [fall]
	case KEY_ENTER: // submit msg, if any
		return blen > 0;
	case KEY_LEFT:
		moveCursorLeft();
		return false;
	case KEY_RIGHT:
		moveCursorRight();
		return false;
	case 127: // also backspace (ascii) [fall]
	case KEY_BACKSPACE:
		if (bidx > 0) {
			memmove(&buffer[bidx - 1], &buffer[bidx], blen - bidx);
			buffer[--blen] = '\0';
			moveCursorLeft();
			refreshUI();
		}
		return false;
	case KEY_HOME:
		moveCursorHome();
		return false;
	case KEY_END:
		moveCursorEnd();
		return false;
	case KEY_RESIZE:
		refreshUI();
		return false;
	}

	if (blen < BUFFER_SIZE) {
		if (bidx < blen)
			memmove(&buffer[bidx + 1], &buffer[bidx], blen - bidx);
		buffer[bidx] = (char) c;
		buffer[++blen] = '\0';
		moveCursorRight();
		refreshUI();
	}

	return false;
}


static enum ChatCmd parseChatCommand(const char* const cmd)
{
	if (strcmp(cmd, "/quit") == 0)
		return CHATCMD_QUIT;
	return CHATCMD_NORMAL;
}


static bool checkfd(const int fd)
{
	struct timeval timeout = { 0, 5000 };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	return select(fd + 1, &fds, NULL, NULL, &timeout) > 0;
}


int chat(const enum ConnectionMode mode)
{
	if ((cinfo = initialize_connection(mode)) == NULL)
		return EXIT_FAILURE;

	initializeUI();
	refreshUI();

	const char *uname = NULL, *msg = NULL;

	for (;;) {

		if (checkfd(cinfo->remote_fd)) {
			readInto(conn_buffer, cinfo->remote_fd, BUFFER_SIZE);
			uname = cinfo->remote_uname;
			msg = conn_buffer;
		} else if (updateTextBox()) {
			writeInto(cinfo->remote_fd, buffer);
			uname = cinfo->local_uname;
			msg = buffer;
		}

		if (uname != NULL && msg != NULL) {
			if (msg[0] == '/' && parseChatCommand(msg) == CHATCMD_QUIT) {
				stackInfo("Connection closed by %s", uname);
				break;
			} else {
				stackMsg(uname, msg);
			}

			if (uname == cinfo->local_uname)
				clearTextBox();
			
			uname = NULL;
			msg = NULL;
			refreshUI();
		}
	}

	refreshUI();
	terminateUI();
	freeMsgStack();
	return EXIT_SUCCESS;
}

