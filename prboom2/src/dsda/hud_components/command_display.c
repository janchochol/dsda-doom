//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Command Display HUD Component
//

#include "d_ticcmd.h"

#include "dsda/build.h"

#include "base.h"

#include "command_display.h"

#define MAX_HISTORY 20

typedef struct {
  signed char forwardmove;
  signed char sidemove;
  signed char angleturn;
  char use;
  char attack;
  char change;
} dsda_command_t;

typedef struct dsda_command_display_s {
  dsda_command_t command;
  int repeat;
  dsda_text_t component;
  char color;
  struct dsda_command_display_s* next;
  struct dsda_command_display_s* prev;
} dsda_command_display_t;

int dsda_command_history_size;
int dsda_hide_empty_commands;

static dsda_command_display_t command_history[MAX_HISTORY];
static dsda_command_display_t* current_command = command_history;
static dsda_command_display_t next_command_display;

static int base_y;

static void dsda_TicCmdToCommand(dsda_command_t* command, ticcmd_t* cmd) {
  command->forwardmove = cmd->forwardmove;
  command->sidemove = cmd->sidemove;
  command->angleturn = (cmd->angleturn + 128) >> 8;

  command->use = command->attack = command->change = 0;

  if (cmd->buttons && !(cmd->buttons & BT_SPECIAL)) {
    if (cmd->buttons & BT_ATTACK)
      command->attack = 1;

    if (cmd->buttons & BT_USE)
      command->use = 1;

    if (cmd->buttons & BT_CHANGE)
      command->change = 1 + ((cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT);
  }
}

static int dsda_IsEmptyCommand(dsda_command_t* command) {
  return command->forwardmove == 0 &&
         command->sidemove == 0 &&
         command->angleturn == 0 &&
         command->use == 0 &&
         command->attack == 0 &&
         command->change == 0;
}

static int dsda_IsCommandEqual(dsda_command_t* command, dsda_command_t* other) {
  return command->forwardmove == other->forwardmove &&
         command->sidemove == other->sidemove &&
         command->angleturn == other->angleturn &&
         command->use == other->use &&
         command->attack == other->attack &&
         command->change == other->change;
}

void dsda_ResetCommandHistory(void) {
  int i;

  for (i = 0; i < MAX_HISTORY; ++i) {
    memset(&command_history[i].command, 0, sizeof(command_history[i].command));
    command_history[i].repeat = 0;
    command_history[i].component.msg[0] = '\0';
    dsda_RefreshHudText(&command_history[i].component);
  }
}

void dsda_InitCommandHistory(void) {
  int i;

  dsda_command_history_size = dsda_IntConfig(dsda_config_command_history_size);
  dsda_hide_empty_commands = dsda_IntConfig(dsda_config_hide_empty_commands);

  for (i = 1; i < MAX_HISTORY; ++i) {
    command_history[i].prev = &command_history[i - 1];
    command_history[i - 1].next = &command_history[i];
  }

  command_history[0].prev = &command_history[MAX_HISTORY - 1];
  command_history[MAX_HISTORY - 1].next = &command_history[0];
}

void dsda_InitCommandDisplayHC(int x_offset, int y_offset, int vpt, int* args, int arg_count) {
  int i;

  for (i = 0; i < MAX_HISTORY; ++i) {
    command_history[i].color = HUlib_Color(CR_GRAY);
    dsda_InitTextHC(&command_history[i].component, x_offset, y_offset + i * 8, vpt);
  }

  next_command_display.color = HUlib_Color(CR_GOLD);
  dsda_InitTextHC(&next_command_display.component, x_offset, y_offset, vpt);

  base_y = next_command_display.component.text.y;
}

static void dsda_UpdateCommandText(dsda_command_t* command,
                                   dsda_command_display_t* display_command, dboolean playback) {
  int length;

  length = sprintf(display_command->component.msg, "\x1b%c", display_command->color);

  if (playback)
    length += sprintf(display_command->component.msg + length, " PL  ");
  else if (display_command->repeat)
    length += sprintf(display_command->component.msg + length, "x%-3d ", display_command->repeat + 1);
  else
    length += sprintf(display_command->component.msg + length, "     ");

  if (command->forwardmove > 0)
    length += sprintf(display_command->component.msg + length, "MF%2d ", command->forwardmove);
  else if (command->forwardmove < 0)
    length += sprintf(display_command->component.msg + length, "MB%2d ", -command->forwardmove);
  else
    length += sprintf(display_command->component.msg + length, "     ");

  if (command->sidemove > 0)
    length += sprintf(display_command->component.msg + length, "SR%2d ", command->sidemove);
  else if (command->sidemove < 0)
    length += sprintf(display_command->component.msg + length, "SL%2d ", -command->sidemove);
  else
    length += sprintf(display_command->component.msg + length, "     ");

  if (command->angleturn > 0)
    length += sprintf(display_command->component.msg + length, "TL%2d ", command->angleturn);
  else if (command->angleturn < 0)
    length += sprintf(display_command->component.msg + length, "TR%2d ", -command->angleturn);
  else
    length += sprintf(display_command->component.msg + length, "     ");

  if (command->attack)
    length += sprintf(display_command->component.msg + length, "A");
  else
    length += sprintf(display_command->component.msg + length, " ");

  if (command->use)
    length += sprintf(display_command->component.msg + length, "U");
  else
    length += sprintf(display_command->component.msg + length, " ");

  if (command->change)
    length += sprintf(display_command->component.msg + length, "C%d", command->change);
  else
    length += sprintf(display_command->component.msg + length, "  ");

  dsda_RefreshHudText(&display_command->component);
}

void dsda_AddCommandToCommandDisplay(ticcmd_t* cmd) {
  dsda_command_t command;

  dsda_TicCmdToCommand(&command, cmd);

  if (dsda_hide_empty_commands && dsda_IsEmptyCommand(&command))
    return;

  if (dsda_IsCommandEqual(&command, &current_command->command))
    ++current_command->repeat;
  else {
    current_command->repeat = 0;
    current_command = current_command->next;
    current_command->command = command;
  }

  dsda_UpdateCommandText(&command, current_command, false);
}

void dsda_UpdateCommandDisplayHC(void) {
  // nothing to do
}

static void dsda_DrawCommandDisplayLine(dsda_command_display_t* command, int offset) {
  command->component.text.y = base_y - 8 * offset;
  dsda_DrawBasicText(&command->component);
}

void dsda_DrawCommandDisplayHC(void) {
  int i;
  int offset = 0;
  dsda_command_display_t* command = current_command;

  if (dsda_BuildMode()) {
    ticcmd_t next_cmd;
    dsda_command_t next_command;

    dsda_CopyBuildCmd(&next_cmd);
    dsda_TicCmdToCommand(&next_command, &next_cmd);
    dsda_UpdateCommandText(&next_command, &next_command_display, dsda_BuildPlayback());
    dsda_DrawCommandDisplayLine(&next_command_display, offset);

    ++offset;
  }

  for (i = 0; i < dsda_command_history_size; ++i) {
    dsda_DrawCommandDisplayLine(command, i + offset);
    command = command->prev;
  }
}
