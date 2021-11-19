// AJBSP <-> EDGE Bridge Code

// These are partially recreated from what was removed from GLBSP as it became AJBSP

// Display Prototypes
typedef enum
{
  DIS_INVALID,        // Nonsense value is always useful
  DIS_BUILDPROGRESS,  // Build Box, has 2 bars
  DIS_FILEPROGRESS,   // File Box, has 1 bar
  NUMOFGUITYPES
}
displaytype_e;

// Callback functions
typedef struct nodebuildfuncs_s
{
  // EDGE I_Printf
  void (* log_printf)(const char *message, ...);

  // EDGE I_Debugf
  void (* log_debugf)(const char *message, ...);

  // EDGE E_NodeProgress
  void (* display_setBar)(int perc);
}
nodebuildfuncs_t;

int AJBSP_Build(const char *filename, const char *outname, const nodebuildfuncs_t *display_funcs);