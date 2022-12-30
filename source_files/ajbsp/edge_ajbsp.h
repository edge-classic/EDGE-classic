//------------------------------------------------------------------------
//  AJ-BSP <-> EDGE INTERFACE
//------------------------------------------------------------------------
//
//  Copyright (C) 2022  Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include <filesystem>

// Callback functions
typedef struct nodebuildfuncs_s
{
  // EDGE I_Printf
  void (* log_printf)(const char *message, ...);

  // EDGE I_Debugf
  void (* log_debugf)(const char *message, ...);

  // EDGE I_Error
  void (* log_error)(const char *message, ...);

  // EDGE E_ProgressMessage
  void (* progress_message)(const char *message);
}
nodebuildfuncs_t;

int AJBSP_Build(std::filesystem::path filename, std::filesystem::path outname, const nodebuildfuncs_t *display_funcs);