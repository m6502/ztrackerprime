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

    // Multichannel .MID: Type 1 file where each zTracker track is its own
    // MIDI track. MIDI channel for each event is preserved from the song's
    // per-instrument channel assignments (encoded into e->data1 by playback).
    int ExportMultichannelMID(char *fn);

    // Per-track .MID: emits one Type 0 file per zTracker track that has
    // events. Output filename per track is derived from fn by stripping a
    // trailing ".mid" and appending "_trackNN.mid".
    int ExportPerTrackMID(char *fn);
};

#endif


