/* 
 *
 *   Copyright (c) 1994, 2002, 2003 Johannes Prix
 *   Copyright (c) 1994, 2002 Reinhard Prix
 *   Copyright (c) 2004-2010 Arthur Huillet
 *
 *
 *  This file is part of Freedroid
 *
 *  Freedroid is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Freedroid is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Freedroid; see the file COPYING. If not, write to the 
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *  MA  02111-1307  USA
 *
 */

/**
 * This file contains all functions dealing with the dialog interface,
 * including blitting the chat log to the screen and drawing the
 * right portrait images to the screen.
 */

#define _chat_c

#include "system.h"

#include "defs.h"
#include "struct.h"
#include "proto.h"
#include "global.h"

#include "widgets/widgets.h"

/* Some forward declarations */
static void run_chat();
static void chat_delete_context(struct chat_context *);

/* Stack of chat contexts. The current context is at the top of the stack */
static struct list_head chat_context_stack = LIST_HEAD_INIT(chat_context_stack);

/**
 * Return the current chat context.
 *
 * The current context is the one at top of the contexts stack.
 *
 * @return A pointer to the current chat context, or NULL if no context is stacked.
 */
struct chat_context *chat_get_current_context(void)
{
	if (list_empty(&chat_context_stack))
		return NULL;

	return (struct chat_context *)list_entry(chat_context_stack.next, struct chat_context, stack_node);
}

/**
 * Push a chat context on top of the stack.
 *
 * The pushed context becomes the current one.
 *
 * @param chat_context Pointer to the chat context to push.
 */
static void chat_push_context(struct chat_context *chat_context)
{
	list_add(&chat_context->stack_node, &chat_context_stack);
}

/** Pop from the chat contexts stack.
 *
 * This function deletes the context at the top of the list.
 * The new top context becomes the current one.
 */
static void chat_pop_context(void)
{
	struct chat_context *top_chat_context = chat_get_current_context();
	if (top_chat_context) {
		list_del(chat_context_stack.next);
		chat_delete_context(top_chat_context);
	}
}

/**
 * Initialize a dialog option to "empty" default values.
 *
 * @param d Pointer to the struct dialogue_option to initialize.
 */
static void init_dialog_option(dialogue_option *d)
{
	d->topic = NULL;
	d->option_text = NULL;
	d->no_text = 0;
	d->lua_code = NULL;
	d->exists = 0;
}

/**
 * Free the memory used by a dialogue_option.
 *
 * @param d Pointer to the struct dialogue_option to free.
 */
static void delete_dialog_option(dialogue_option *d)
{
	free(d->topic);
	free(d->option_text);
	free(d->lua_code);
}

/**
 * Create a chat context and initialize it.
 *
 * @param partner Pointer to the enemy to talk with
 * @param npc Pointer to the npc containing the dialogue definition.
 * @param is_subdialog True if the dialog to start is a subdialog.
 *
 * @return Pointer to the allocated chat context.
 */
static struct chat_context *chat_create_context(enemy *partner, struct npc *npc, int is_subdialog)
{
	int i;
	struct chat_context *new_chat_context = (struct chat_context *)MyMalloc(sizeof(struct chat_context));

	// Init chat control variables.
	// MyMalloc() already zeroed the struct, so we only initialize 'non zero' fields.

	new_chat_context->is_subdialog = is_subdialog;
	new_chat_context->partner = partner;
	new_chat_context->npc = npc;
	new_chat_context->partner_started = partner->will_rush_tux;

	// topic_stack[0] is never replaced, so a static allocation is enough.
	new_chat_context->topic_stack[0] = "";

	for (i = 0; i < MAX_DIALOGUE_OPTIONS_IN_ROSTER; i++) {
		init_dialog_option(&new_chat_context->dialog_options[i]);
	}

	// Only run the init script the first time the dialog is open.
	// An init script on a subdialog does not really make sense, so it is just
	// ignored.
	// The startup script is run every time a dialog or a subdialog is open.
	new_chat_context->state = RUN_INIT_SCRIPT;
	if (npc->chat_character_initialized || is_subdialog) {
		new_chat_context->state = RUN_STARTUP_SCRIPT;
	}

	// dialog_flags points to the npc chat_flags array, so that currently
	// active dialog options are saved thanks to the save of the npc
	// structures.
	// However, subdialogs are not referenced by the npc, and so their states
	// can not be saved. A 'dummy' chat_flags array is thus needed during the
	// run of a subdialog, that dummy array being lost when the subdialog is
	// closed.
	if (!is_subdialog) {
		new_chat_context->dialog_flags = &npc->chat_flags[0];
		// Ensure the initialization of the option flags on the first run of
		// the dialog.
		if (!npc->chat_character_initialized) {
			for (i = 0; i < MAX_DIALOGUE_OPTIONS_IN_ROSTER; i++) {
				new_chat_context->dialog_flags[i] = 0;
			}
		}
	} else {
		uint8_t *dummy_chat_flags = (uint8_t *)MyMalloc(sizeof(uint8_t)*MAX_DIALOGUE_OPTIONS_IN_ROSTER);
		new_chat_context->dialog_flags = dummy_chat_flags;
	}

	// No dialog option currently selected by the user
	new_chat_context->current_option = -1;

	return new_chat_context;
}

/**
 * Delete a chat_context (free all the memory allocations)
 *
 * @param chat_context Pointer to the caht_context to delete.
 */
static void chat_delete_context(struct chat_context *chat_context)
{
	int i;

	free(chat_context->initialization_code);
	free(chat_context->startup_code);

	// topic_stack[0] is not dynamically allocated, so there is no need to free it.
	for (i = chat_context->topic_stack_slot; i > 0; i--)
		free(chat_context->topic_stack[chat_context->topic_stack_slot]);

	for (i = 0; i < MAX_DIALOGUE_OPTIONS_IN_ROSTER; i++) {
		delete_dialog_option(&chat_context->dialog_options[i]);
	}

	if (chat_context->is_subdialog) {
		free(chat_context->dialog_flags);
	}

	free(chat_context);
}

/**
 * \brief Push a new topic in the topic stack.
 * \param topic The topic name.
 */
void chat_push_topic(const char *topic)
{
	struct chat_context *current_chat_context = chat_get_current_context();

	if (current_chat_context->topic_stack_slot < CHAT_TOPIC_STACK_SIZE - 1) {
		current_chat_context->topic_stack[++current_chat_context->topic_stack_slot] = strdup(topic);
	} else {
		ErrorMessage(__FUNCTION__,
			     "The maximum depth of %hd topics has been maxed out.",
			     PLEASE_INFORM, IS_WARNING_ONLY, CHAT_TOPIC_STACK_SIZE);
	}
}

/**
 * \brief Pop the top topic in the topic stack.
 */
void chat_pop_topic()
{
	struct chat_context *current_chat_context = chat_get_current_context();

	if (current_chat_context->topic_stack_slot > 0) {
		free(current_chat_context->topic_stack[current_chat_context->topic_stack_slot]);
		current_chat_context->topic_stack[current_chat_context->topic_stack_slot] = NULL;
		current_chat_context->topic_stack_slot--;
	} else {
		ErrorMessage(__FUNCTION__,
			     "Root topic can't be popped.",
			     PLEASE_INFORM, IS_WARNING_ONLY);
	}
}

/**
 * This function should load new chat dialogue information from the 
 * chat info file into the chat roster.
 *
 */
static void load_dialog(const char *fpath, struct chat_context *context)
{
	char *ChatData;
	char *SectionPointer;
	char *EndOfSectionPointer;
	int i;
	int OptionIndex;
	int NumberOfOptionsInSection;
	char *text;

#define CHAT_CHARACTER_BEGIN_STRING "Beginning of new chat dialog for character=\""
#define CHAT_CHARACTER_END_STRING "End of chat dialog for character"
#define NEW_OPTION_BEGIN_STRING "\nNr="
#define NO_TEXT_OPTION_STRING "NO_TEXT"

	// Read the whole chat file information into memory
	ChatData = ReadAndMallocAndTerminateFile(fpath, CHAT_CHARACTER_END_STRING);
	SectionPointer = ChatData;

	// Read the initialization code
	//
	context->initialization_code = ReadAndMallocStringFromDataOptional(SectionPointer, "<FirstTime LuaCode>", "</LuaCode>");
	context->startup_code = ReadAndMallocStringFromDataOptional(SectionPointer, "<EveryTime LuaCode>", "</LuaCode>");

	// At first we go take a look on how many options we have
	// to decode from this section.
	//
	NumberOfOptionsInSection = CountStringOccurences(SectionPointer, NEW_OPTION_BEGIN_STRING);

	// Now we see which option index is assigned to this option.
	// It may happen, that some numbers are OMITTED here!  This
	// should be perfectly ok and allowed as far as the code is
	// concerned in order to give the content writers more freedom.
	//
	for (i = 0; i < NumberOfOptionsInSection; i++) {
		SectionPointer = LocateStringInData(SectionPointer, NEW_OPTION_BEGIN_STRING);
		ReadValueFromString(SectionPointer, NEW_OPTION_BEGIN_STRING, "%d",
				    &OptionIndex, SectionPointer + strlen(NEW_OPTION_BEGIN_STRING) + 50);

		SectionPointer++;

		// Find the end of this dialog option
		EndOfSectionPointer = strstr(SectionPointer, NEW_OPTION_BEGIN_STRING);

		if (EndOfSectionPointer) {	//then we are not the last option
			*EndOfSectionPointer = 0;
		}
		// Anything that is loaded into the chat roster doesn't need to be freed,
		// cause this will be done by the next 'InitChatRoster' function anyway.
		//
		text = ReadAndMallocStringFromDataOptional(SectionPointer, "Text=\"", "\"");
		if (!text) {
			text = ReadAndMallocStringFromData(SectionPointer, "Text=_\"", "\"");
		}
		context->dialog_options[OptionIndex].option_text = L_(text);

		text = ReadAndMallocStringFromDataOptional(SectionPointer, "Topic=\"", "\"");
		if (text) {
			context->dialog_options[OptionIndex].topic = text;
		} else {
			// topic content has to be malloc'd.
			context->dialog_options[OptionIndex].topic = strdup("");
		}

		context->dialog_options[OptionIndex].no_text = strstr(SectionPointer, NO_TEXT_OPTION_STRING) > 0;

		context->dialog_options[OptionIndex].lua_code = ReadAndMallocStringFromDataOptional(SectionPointer, "<LuaCode>", "</LuaCode>");

		context->dialog_options[OptionIndex].exists = 1;

		if (EndOfSectionPointer)
			*EndOfSectionPointer = NEW_OPTION_BEGIN_STRING[0];
	}

	// Now we've got all the information we wanted from the dialogues file.
	// We can now free the loaded file again.  Upon a new character dialogue
	// being initiated, we'll just reload the file.  This is very convenient,
	// for it allows making and testing changes to the dialogues without even
	// having to restart FreedroidRPG!  Very cool!
	//
	free(ChatData);
}

static void show_chat_up_button(void)
{
	ShowGenericButtonFromList(CHAT_LOG_SCROLL_UP_BUTTON);
}

static void show_chat_down_button(void)
{
	ShowGenericButtonFromList(CHAT_LOG_SCROLL_DOWN_BUTTON);
}

/**
 * During the Chat with a friendly droid or human, there is a window with
 * the full text transcript of the conversation so far.  This function is
 * here to display said text window and it's content, scrolled to the
 * position desired by the player himself.
 */
void show_chat_log(enemy *chat_enemy)
{
	struct image *img = get_droid_portrait_image(chat_enemy->type);
	float scale;
	moderately_finepoint pos;

	blit_background("conversation.png");

	// Compute the maximum uniform scale to apply to the bot image so that it fills
	// the droid portrait image, and center the image.
	scale = min((float)Droid_Image_Window.w / (float)img->w, (float)Droid_Image_Window.h / (float)img->h);
	pos.x = (float)Droid_Image_Window.x + ((float)Droid_Image_Window.w - (float)img->w * scale) / 2.0;
	pos.y = (float)Droid_Image_Window.y + ((float)Droid_Image_Window.h - (float)img->h * scale) / 2.0;

	display_image_on_screen(img, pos.x, pos.y, IMAGE_SCALE_TRANSFO(scale));

	chat_log.content_above_func = show_chat_up_button;
	chat_log.content_below_func = show_chat_down_button;
	ShowGenericButtonFromList(CHAT_LOG_SCROLL_OFF_BUTTON);
	ShowGenericButtonFromList(CHAT_LOG_SCROLL_OFF2_BUTTON);

	widget_text_display(WIDGET(&chat_log));
}

/**
 * Wait for a mouse click before continuing.
 */
static void wait_for_click(enemy *chat_droid)
{
	SDL_Event event;

	// Rectangle for the player response area
	SDL_Rect clip;
	clip.x = UNIVERSAL_COORD_W(37);
	clip.y = UNIVERSAL_COORD_H(336);
	clip.w = UNIVERSAL_COORD_W(640 - 70);
	clip.h = UNIVERSAL_COORD_H(118);

	StoreMenuBackground(0);

	while (1) {
		RestoreMenuBackground(0);
		SetCurrentFont(Menu_BFont);
		display_text(_("\1Click anywhere to continue..."), clip.x, clip.y, &clip);
		blit_mouse_cursor();
		our_SDL_flip_wrapper();
		SDL_Delay(1);

		SDL_WaitEvent(&event);

		if (event.type == SDL_QUIT) {
			Terminate(EXIT_SUCCESS, TRUE);
		}

		if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == 1)
			goto wait_for_click_return;

		else if (event.type == SDL_KEYDOWN) {
			switch (event.key.keysym.sym) {
			case SDLK_SPACE:
			case SDLK_RETURN:
			case SDLK_ESCAPE:
				goto wait_for_click_return;
			default:
				break;
			}
		}
	}

wait_for_click_return:
	while (EscapePressed() || MouseLeftPressed() || SpacePressed())
		;
	return;
}

void chat_add_response(const char *response)
{
	autostr_append(chat_log.text, "%s", response);
	chat_log.scroll_offset = 0;
}

static int create_and_stack_context(enemy *partner, struct npc *npc, const char *filename, int is_subdialog)
{
	char fpath[2048];
	char finaldir[50];
	struct chat_context *new_context = NULL;

	new_context = chat_create_context(partner, npc, is_subdialog);

	// Find the dialog file and load it inside the new chat context
	sprintf(finaldir, "%s", DIALOG_DIR);
	find_file(filename, finaldir, fpath, 0);
	load_dialog(fpath, new_context);

	// Push the new chat_context
	chat_push_context(new_context);

	return TRUE;
}

/**
 * Create and push a chat context to run a sub dialog.
 *
 * @param filename Subdialog filename.
 *
 * @return TRUE on success, FALSE on error.
 */
int stack_subdialog(const char *filename)
{
	// A subdialog uses the same partner and npc than its parent dialog.
	// The parent dialog is the one on top of the chat_context stack.
	struct chat_context *current_chat_context = chat_get_current_context();

	return create_and_stack_context(current_chat_context->partner, current_chat_context->npc, filename, TRUE);
}

/**
 * Create and push a chat context to run a dialog.
 *
 * @param partner Enemy we are chatting with.
 *
 * @return TRUE on success, FALSE on error.
 */
int stack_dialog(enemy *partner)
{
	struct npc *npc = npc;
	char dialog_filename[5000];

	npc = npc_get(partner->dialog_section_name);
	if (!npc)
		return FALSE;

	strcpy(dialog_filename, partner->dialog_section_name);
	strcat(dialog_filename, ".dialog");

	return create_and_stack_context(partner, npc, dialog_filename, FALSE);
}

/**
 * This function performs a menu for the player to select from, using the
 * keyboard or mouse wheel.
 */
static int chat_do_menu_selection_filtered(struct chat_context *context)
{
	int MenuSelection;
	char *FilteredChatMenuTexts[MAX_DIALOGUE_OPTIONS_IN_ROSTER];
	int i;
	int use_counter = 0;
	char *topic = context->topic_stack[context->topic_stack_slot];

	// We filter out those answering options that are allowed by the flag mask
	for (i = 0; i < MAX_DIALOGUE_OPTIONS_IN_ROSTER; i++) {
		FilteredChatMenuTexts[i] = "";
		if (context->dialog_flags[i] && !(strcmp(topic, context->dialog_options[i].topic))) {
			FilteredChatMenuTexts[use_counter] = context->dialog_options[i].option_text;
			use_counter++;
		}
	}

	// Now we do the usual menu selection, using only the activated chat alternatives...
	//
	MenuSelection = chat_do_menu_selection(FilteredChatMenuTexts, context->partner);

	// Now that we have an answer, we must transpose it back to the original array
	// of all theoretically possible answering possibilities.
	//
	if (MenuSelection != (-1)) {
		use_counter = 0;
		for (i = 0; i < MAX_DIALOGUE_OPTIONS_IN_ROSTER; i++) {

			if (context->dialog_flags[i] && !(strcmp(topic, context->dialog_options[i].topic))) {
				use_counter++;
				if (MenuSelection == use_counter) {
					DebugPrintf(1, "\nOriginal MenuSelect: %d. \nTransposed MenuSelect: %d.", MenuSelection, i + 1);
					return (i + 1);
				}
			}
		}
	}

	return (MenuSelection);
}

/**
 * Process the chat option selected by the user.
 *
 * - Echo the option text of the chosen option, unless NO_TEXT is set.
 * - Run the associated LUA code.
 */
static void process_chat_option(struct chat_context *context)
{
	int current_selection = context->current_option;

	// Reset chat control variables for this option, so the user will have to
	// select a new option, unless the lua code ends the dialog or
	// auto-activates an other option.
	context->end_dialog = 0;
	context->current_option = -1;

	// Echo option text
	if (!context->dialog_options[current_selection].no_text) {
		chat_add_response("\1- ");
		chat_add_response(L_(context->dialog_options[current_selection].option_text));
		chat_add_response("\n");
	}

	// Run LUA associated with this selection
	if (context->dialog_options[current_selection].lua_code && strlen(context->dialog_options[current_selection].lua_code))
		context->lua_coroutine = load_lua_coroutine(LUA_DIALOG, context->dialog_options[current_selection].lua_code);
}

/**
 * Run the dialog defined by the chat contest on top of the stack.
 *
 * This is the most important subfunction of the whole chat with friendly
 * droids and characters.  After a chat context has been filled and pushed on
 * the stack, this function is invoked to handle the actual chat interaction
 * and the dialog flow.
 */
static void run_chat()
{
	struct chat_context *current_chat_context = NULL;

	// Initialize the chat log widget.
	widget_set_rect(WIDGET(&chat_log), CHAT_SUBDIALOG_WINDOW_X, CHAT_SUBDIALOG_WINDOW_Y, CHAT_SUBDIALOG_WINDOW_W, CHAT_SUBDIALOG_WINDOW_H);
	chat_log.font = FPS_Display_BFont;
	chat_log.line_height_factor = LINE_HEIGHT_FACTOR;

	Activate_Conservative_Frame_Computation();

	// Dialog engine main code.
	//
	// A dialog flow is decomposed into a sequence of several steps:
	// 1 - run the initialization lua code (only once per game)
	// 2 - run the startup lua code (each time a dialog is launched)
	// 3 - interaction step: let the user chose a dialog option, and run the
	//     associated lua script
	// 4 - loop on the next interaction step, unless the dialog is ended.
	//
	// In some cases, a lua script needs a user's interaction (wait for a click
	// in lua npc_say(), for instance). We then have to interrupt the lua script,
	// run an interaction loop, and resume the lua script after the user's interaction.
	//
	// To implement a script interruption/resuming, we use the co-routine
	// yield/resume features of Lua. When a lua script is run, we thus have
	// to loop on its execution until it ends.
	//
	// For calls to a subdialog (lua run_subdialog()) or an other inner dialog
	// (lua start_dialog()), the current lua script also has to be interrupted,
	// the new dialog has to be extracted from the stack and run, and then the
	// previous dialog script has to be resumed.
	// at each loop step.
	//
	// All of these apparently intricate loops are implemented with one single
	// loop, taking care of the actual state of the chat, restarting the loop
	// when needed, and extracting the top chat context at each loop step :
	//
	// while (there is something to run)
	//   get chat context from top of stack
	//   if (a user interaction is waited)
	//     wait user interaction
	//   else if (a lua co-routine is started)
	//     resume the lua co-routine
	//   else
	//     execute dialog FSM step
	//   if (current dialog is ended)
	//     pop the chat context stack

	// Implement the former algorithm, but using 'continue' statements
	// instead of 'else' clauses.
	while ((current_chat_context = chat_get_current_context())) {

		/*
		 * 1- Chat screen rendering, and user's interaction
		 */

		show_chat_log(current_chat_context->partner);

		if (current_chat_context->wait_user_click) {
			wait_for_click(current_chat_context->partner);
			current_chat_context->wait_user_click = FALSE;

			// restart the loop, to re-render the chat screen
			continue;
		}

		/*
		 * 2- Resume the current lua coroutine, if any, until it ends.
		 */

		if (current_chat_context->lua_coroutine) {
			int rtn = resume_lua_coroutine(current_chat_context->lua_coroutine);

			// If rtn is TRUE, the lua script reached its end, so we mark it
			// as such.
			if (rtn) {
				current_chat_context->lua_coroutine = NULL;
				if (current_chat_context->end_dialog)
					goto wait_click_and_out;
			}

			// restart the loop, to handle user interaction, if needed, and
			// resume the co-routine, if needed.
			continue;
		}

		/*
		 * 3- Execute current state of the chat FSM
		 */

		switch (current_chat_context->state) {
			case RUN_INIT_SCRIPT:
			{
				if (!current_chat_context->is_subdialog)
					widget_text_init(&chat_log, _("\3--- Start of Dialog ---\n"));

				if (current_chat_context->initialization_code && strlen(current_chat_context->initialization_code)) {
					current_chat_context->lua_coroutine = load_lua_coroutine(LUA_DIALOG, current_chat_context->initialization_code);
				}
				current_chat_context->state = RUN_STARTUP_SCRIPT; // next step once the script ends
				break;
			}
			case RUN_STARTUP_SCRIPT:
			{
				if (!current_chat_context->is_subdialog && current_chat_context->npc->chat_character_initialized)
					widget_text_init(&chat_log, _("\3--- Start of Dialog ---\n"));

				if (current_chat_context->startup_code && strlen(current_chat_context->startup_code)) {
					current_chat_context->lua_coroutine = load_lua_coroutine(LUA_DIALOG, current_chat_context->startup_code);
				}
				current_chat_context->state = SELECT_NEXT_NODE; // next step once the script ends
				break;
			}
			case SELECT_NEXT_NODE:
			{
				// If no dialog is currently selected, let the user choose one of the
				// active options.
				if (current_chat_context->current_option == -1) {
					current_chat_context->current_option = chat_do_menu_selection_filtered(current_chat_context);
					// We do some correction of the menu selection variable:
					// The first entry of the menu will give a 1 and so on and therefore
					// we need to correct this to more C style.
					//
					current_chat_context->current_option--;
				}

				if ((current_chat_context->current_option >= MAX_DIALOGUE_OPTIONS_IN_ROSTER) || (current_chat_context->current_option < 0)) {
					DebugPrintf(0, "%s: Error: chat_control current_node %i out of range!\n", __FUNCTION__, current_chat_context->current_option);
					goto wait_click_and_out;
				}

				// Output the 'partner' message, and load the node script into
				// a lua coroutine.
				process_chat_option(current_chat_context);

				break;
			}
			default:
				break;
		}

		// restart the loop, to handle user interaction, if needed, resume a
		// lua co-routine, if needed, and stepping the FSM.
		continue;

wait_click_and_out:
		while (EscapePressed() || MouseLeftPressed() || SpacePressed());

		// The dialog has ended. Mark the npc as already initialized, to avoid
		// its initialization script to be run again the next time the dialog
		// is open, and pop the dialog from the chat context stack.
		if (!current_chat_context->npc->chat_character_initialized)
			current_chat_context->npc->chat_character_initialized = TRUE;

		chat_pop_context();
	}
}

/**
 * This is more or less the 'main' function of the chat with friendly 
 * droids and characters.  It is invoked directly from the user interface
 * function as soon as the player requests communication or there is a
 * friendly bot who rushes Tux and opens talk.
 */
void ChatWithFriendlyDroid(enemy * ChatDroid)
{
	if (!stack_dialog(ChatDroid))
		return;

	run_chat();
}

/**
 * Validate dialogs syntax and Lua code 
 * This validator loads all known dialogs, checking them for syntax. 
 * It tries to compile each Lua code snippet in the dialogs, checking them for syntax as well.
 * It then proceeds to executing each Lua code snippet, which makes it possible to check for some errors
 * such as typos in function calls (calls to non existing functions). However, this runtime check does not cover 100% of 
 * the Lua code because of branches (when it encounters a if, it will not test both the "then" and "else" branches).
 *
 * As a result, the fact that the validator finds no error does not imply there are no errors in dialogs. 
 * Syntax is checked fully, but runtime validation cannot check all of the code.
 */
int validate_dialogs()
{
	int j, k;
	const char *basename;
	char filename[1024];
	char fpath[2048];
	enemy *dummy_partner;
	struct npc *n;
	int error_caught = FALSE;

	skip_initial_menus = 1;

	/* Disable sound to speed up validation. */
	int old_sound_on = sound_on;
	sound_on = FALSE;

	find_file("levels.dat", MAP_DIR, fpath, 0);
	LoadShip(fpath, 0);
	PrepareStartOfNewCharacter("NewTuxStartGameSquare");

	/* Temporarily disable screen fadings to speed up validation. */
	GameConfig.do_fadings = FALSE;

	/* _says functions are not run by the validator, as they display
	   text on screen and wait for clicks */
	run_lua(LUA_DIALOG, "function npc_says(a)\nend\n");
	run_lua(LUA_DIALOG, "function tux_says(a)\nend\n");
	run_lua(LUA_DIALOG, "function cli_says(a)\nend\n");

	/* Subdialogs currently call run_chat and we cannot do that when validating dialogs */
	run_lua(LUA_DIALOG, "function run_subdialog(a)\nend\n");
	run_lua(LUA_DIALOG, "function start_chat(a)\nend\n");

	/* Shops must not be run (display + wait for clicks) */
	run_lua(LUA_DIALOG, "function trade_with(a)\nend\n");

	/* drop_dead cannot be tested because it means we would try to kill our dummy bot
	 several times, which is not allowed by the engine */
	run_lua(LUA_DIALOG, "function drop_dead(a)\nend\n");

	run_lua(LUA_DIALOG, "function user_input_string(a)\nreturn \"dummy\";\nend\n");

	run_lua(LUA_DIALOG, "function upgrade_items(a)\nend\n");
	run_lua(LUA_DIALOG, "function craft_addons(a)\nend\n");
	/* set_bot_destination cannot be tested because this may be invoked when the bot is
	 on a different level than where the bot starts */
	run_lua(LUA_DIALOG, "function set_bot_destination(a)\nend\n");

	/* takeover requires user input - hardcode it to win */
	run_lua(LUA_DIALOG, "function takeover(a)\nreturn true\nend\n");

	/* push_topic() and pop_topic() cannot be tested because the validator doesn't follow 
	 the logical order of nodes */
	run_lua(LUA_DIALOG, "function topic(a)\nend\n");
	run_lua(LUA_DIALOG, "function pop_topic(a)\nend\n");

	/* This dummy will be used to test break_off_and_attack() functions and such. */
	BROWSE_ALIVE_BOTS(dummy_partner) {
		break;
	}

	list_for_each_entry(n, &npc_head, node) {
		struct chat_context *dummy_context = chat_create_context(dummy_partner, n, FALSE);

		basename = n->dialog_basename;
		printf("Testing dialog \"%s\"...\n", basename);
		sprintf(filename, "%s.dialog", basename);
		find_file(filename, DIALOG_DIR, fpath, 0);
		load_dialog(fpath, dummy_context);

		chat_push_context(dummy_context);

		if (dummy_context->initialization_code && strlen(dummy_context->initialization_code)) {
			printf("\tinit code\n");
			run_lua(LUA_DIALOG, dummy_context->initialization_code);
		}

		if (dummy_context->startup_code && strlen(dummy_context->startup_code)) {
			printf("\tstartup code\n");
			run_lua(LUA_DIALOG, dummy_context->startup_code);
		}

		k = 0;
		for (j = 0; j < MAX_DIALOGUE_OPTIONS_IN_ROSTER; j++) {
			if (dummy_context->dialog_options[j].lua_code && strlen(dummy_context->dialog_options[j].lua_code)) {
				if (!(k%5)) {
					printf("\n");
				}
				printf("|node %2d|\t", j);
				run_lua(LUA_DIALOG, dummy_context->dialog_options[j].lua_code);
				k++;
			}
		}

		printf("\n... dialog OK\n\n");

		chat_pop_context();
	}

	/* Re-enable sound as needed. */
	sound_on = old_sound_on;

	/* Re-enable screen fadings. */
	GameConfig.do_fadings = TRUE;

	return error_caught;
}

#undef _chat_c
