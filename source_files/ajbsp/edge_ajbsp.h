// AJBSP <-> EDGE Bridge Code

// This is partially recreated from what was removed from GLBSP as it became AJBSP

// Callback functions
typedef struct nodebuildfuncs_s
{
  // EDGE I_Printf
  void (* log_printf)(const char *message, ...);

  // EDGE I_Debugf
  void (* log_debugf)(const char *message, ...);

  // EDGE E_ProgressMessage
  void (* progress_message)(const char *message);
}
nodebuildfuncs_t;

int AJBSP_Build(const char *filename, const char *outname, const nodebuildfuncs_t *display_funcs);