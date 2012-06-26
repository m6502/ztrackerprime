#ifndef _IMPORT_EXPORT_H
#define _IMPORT_EXPORT_H


#include "zt.h"
#include "it.h"

class ZTImportExport {

public:
    ZTImportExport();
    ~ZTImportExport();

    int ImportIT(char *fn, zt_module* zt);
    int ExportIT(char *fn, zt_module* zt) { }

    int ExportMID(char *fn, int format); // format = 0 or 1, for MIDI format 0 or 1
    


};


#endif


