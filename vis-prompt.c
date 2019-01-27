#include <string.h>
#include "vis-core.h"
#include "text-motions.h"
#include "text-objects.h"
#include "text-util.h"

bool vis_prompt_cmd(Vis *vis, const char *cmd) {
	if (!cmd || !cmd[0] || !cmd[1])
		return true;
	switch (cmd[0]) {
	case '/':
		return vis_motion(vis, VIS_MOVE_SEARCH_FORWARD, cmd+1);
	case '?':
		return vis_motion(vis, VIS_MOVE_SEARCH_BACKWARD, cmd+1);
	case '+':
	case ':':
		register_put0(vis, &vis->registers[VIS_REG_COMMAND], cmd+1);
		return vis_cmd(vis, cmd+1);
	default:
		return false;
	}
}

static void prompt_hide(Win *win) {
	Text *txt = win->file->text;
	size_t size = text_size(txt);
	/* make sure that file is new line terminated */
	char lastchar = '\0';
	if (size >= 1 && text_byte_get(txt, size-1, &lastchar) && lastchar != '\n')
		text_insert(txt, size, "\n", 1);
	Filerange lastrange = text_object_line(txt, text_size(txt)-1);
	char *lastline = text_bytes_alloc0(txt, lastrange.start, text_range_size(&lastrange));
	if (lastline) {
		if (lastline[0] == '\n' || (strchr(":/?", lastline[0]) && (lastline[1] == '\n' || lastline[1] == '\0')))
			text_delete_range(txt, &lastrange);
		size_t pos = lastrange.start;
		size_t prev = pos;
		while (pos > (prev = text_line_prev(txt, pos))) {
			pos = prev;
			Filerange range = text_object_line(txt, pos);
			if (text_range_size(&range) > 0) {
				char *histline = text_bytes_alloc0(txt, range.start, text_range_size(&range));
				if (histline[0] == '\n' || (strchr(":/?", histline[0]) && (histline[1] == '\n' || histline[1] == '\0')) || \
					!strncmp(histline, lastline, strlen(histline)))
					text_delete_range(txt, &range);
				free(histline);
			}
		}
	}
	free(lastline);
	vis_window_close(win);
}

static void prompt_restore(Win *win) {
	Vis *vis = win->vis;
	/* restore window and mode which was active before the prompt window
	 * we deliberately don't use vis_mode_switch because we do not want
	 * to invoke the modes enter/leave functions */
	if (win->parent)
		vis->win = win->parent;
	vis->mode = win->parent_mode;
}

static const char *prompt_enter(Vis *vis, const char *keys, const Arg *arg) {
	Win *prompt = vis->win;
	View *view = prompt->view;
	Text *txt = prompt->file->text;
	Win *win = prompt->parent;
	char *cmd = NULL;
	const char *prefix = NULL;

	Filerange range = view_selection_get(view);
	if (prompt->file == vis->command_file)
		prefix = ":";
	else if (prompt->file == vis->search_file)
		prefix = "/?";
	if (!vis->mode->visual) {
		size_t pos = view_cursor_get(view);
		range = text_object_line(txt, pos);
	}
	if (text_range_size(&range) > 1)
		cmd = text_bytes_alloc0(txt, range.start, text_range_size(&range));
	if (cmd && !strchr(prefix, cmd[0])) {
		free(cmd);
		cmd = NULL;
	}

	if (!win || !cmd) {
		if (!win)
			vis_info_show(vis, "Prompt window invalid");
		else if (!cmd)
			vis_info_show(vis, "Failed to detect command");
		prompt_restore(prompt);
		text_delete_range(txt, &range);
		prompt_hide(prompt);
		free(cmd);
		return keys;
	}

	size_t len = strlen(cmd);
	if (len > 0 && cmd[len-1] == '\n')
		cmd[len-1] = '\0';

	prompt_restore(prompt);
	if (vis_prompt_cmd(vis, cmd)) {
		if (range.end != text_size(txt))
			text_appendf(txt, "%s\n", cmd);
		prompt_hide(prompt);
	} else {
		vis->win = prompt;
		vis->mode = &vis_modes[VIS_MODE_INSERT];
	}
	free(cmd);
	vis_draw(vis);
	return keys;
}

static const char *prompt_esc(Vis *vis, const char *keys, const Arg *arg) {
	Win *prompt = vis->win;
	if (view_selections_count(prompt->view) > 1) {
		view_selections_dispose_all(prompt->view);
	} else {
		prompt_restore(prompt);
		prompt_hide(prompt);
	}
	return keys;
}

static const KeyBinding prompt_enter_binding = {
	.key = "<Enter>",
	.action = &(KeyAction){
		.func = prompt_enter,
	},
};

static const KeyBinding prompt_esc_binding = {
	.key = "<Escape>",
	.action = &(KeyAction){
		.func = prompt_esc,
	},
};

static const KeyBinding prompt_tab_binding = {
	.key = "<Tab>",
	.alias = "<C-x><C-o>",
};

void vis_prompt_show(Vis *vis, const char *title) {
	Win *active = vis->win;
	if (active->file != vis->command_file && active->file != vis->search_file) {
		Win *prompt = window_new_file(vis, title[0] == ':' ? vis->command_file : vis->search_file,
			UI_OPTION_ONELINE | UI_OPTION_SYMBOL_EOF);
		if (!prompt)
			return;
		Text *txt = prompt->file->text;
		text_appendf(txt, "%s\n", title);
		Selection *sel = view_selections_primary_get(prompt->view);
		view_cursors_scroll_to(sel, text_size(txt)-1);
		prompt->parent = active;
		prompt->parent_mode = vis->mode;
		vis_window_mode_map(prompt, VIS_MODE_NORMAL, true, "<Enter>", &prompt_enter_binding);
		vis_window_mode_map(prompt, VIS_MODE_INSERT, true, "<Enter>", &prompt_enter_binding);
		vis_window_mode_map(prompt, VIS_MODE_INSERT, true, "<C-j>", &prompt_enter_binding);
		vis_window_mode_map(prompt, VIS_MODE_VISUAL, true, "<Enter>", &prompt_enter_binding);
		vis_window_mode_map(prompt, VIS_MODE_NORMAL, true, "<Escape>", &prompt_esc_binding);
		if (CONFIG_LUA)
			vis_window_mode_map(prompt, VIS_MODE_INSERT, true, "<Tab>", &prompt_tab_binding);
	}
	vis_mode_switch(vis, VIS_MODE_INSERT);
}

void vis_info_show(Vis *vis, const char *msg, ...) {
	va_list ap;
	va_start(ap, msg);
	vis->ui->info(vis->ui, msg, ap);
	va_end(ap);
}

void vis_info_hide(Vis *vis) {
	vis->ui->info_hide(vis->ui);
}

void vis_message_show(Vis *vis, const char *msg) {
	if (!msg)
		return;
	if (!vis->message_window)
		vis->message_window = window_new_file(vis, vis->error_file, UI_OPTION_STATUSBAR);
	Win *win = vis->message_window;
	if (!win)
		return;
	Text *txt = win->file->text;
	size_t pos = text_size(txt);
	text_appendf(txt, "%s\n", msg);
	text_save(txt, NULL);
	view_cursor_to(win->view, pos);
}

void vis_message_hide(Vis *vis) {
	if (!vis->message_window)
		return;
	vis_window_close(vis->message_window);
	vis->message_window = NULL;
}
