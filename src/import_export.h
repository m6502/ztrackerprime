#ifndef _IMPORT_EXPORT_H
#define _IMPORT_EXPORT_H

class zt_module ;


class ZTImportExport
{

public:
    ZTImportExport() = default ;
    ~ZTImportExport() = default ;

    int ImportIT(char *fn, zt_module* zt);
    //int ExportIT(char *fn, zt_module* zt) { }

    int ExportMID(char *fn, int format); // format = 0 or 1, for MIDI format 0 or 1
};

#endif


