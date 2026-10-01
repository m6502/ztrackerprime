// Index probe used only by the drawing benchmark, not the application.
#include "zt.h"

event *indexed_event(event **index, int row) { return index[row]; }
