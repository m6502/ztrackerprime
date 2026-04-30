/*
    Wrapper winmm
*/

#ifndef ZT_WINMM_COMPAT_H
#define ZT_WINMM_COMPAT_H

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

// SysEx send (Windows / WinMM). bytes must include 0xF0 prefix and 0xF7
// terminator. Wraps midiOutLongMsg with the required MIDIHDR prepare /
// unprepare dance. Synchronous: blocks until WinMM accepts the buffer
// (typical OS-level call; not "blocks until receiver consumed it").
static inline MMRESULT zt_midi_out_sysex(HMIDIOUT h, const unsigned char *bytes, int len) {
    if (!h || !bytes || len <= 0) return MMSYSERR_ERROR;
    MIDIHDR hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.lpData = (LPSTR)bytes;
    hdr.dwBufferLength = (DWORD)len;
    hdr.dwBytesRecorded = (DWORD)len;
    if (midiOutPrepareHeader(h, &hdr, sizeof(hdr)) != MMSYSERR_NOERROR) {
        return MMSYSERR_ERROR;
    }
    MMRESULT r = midiOutLongMsg(h, &hdr, sizeof(hdr));
    midiOutUnprepareHeader(h, &hdr, sizeof(hdr));
    return r;
}
#else

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__)
#include <alsa/asoundlib.h>
#elif defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

typedef uint32_t MMRESULT;
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef uintptr_t UINT_PTR;
typedef void *HMIDIIN;
typedef void *HMIDIOUT;
typedef char *LPSTR;

typedef struct tagMIDIHDR {
    LPSTR lpData;
    DWORD dwBufferLength;
    DWORD dwBytesRecorded;
    DWORD dwFlags;
} MIDIHDR, *LPMIDIHDR;

typedef struct tagMIDIOUTCAPS {
    char szPname[128];
} MIDIOUTCAPS;

typedef struct tagMIDIINCAPS {
    char szPname[128];
} MIDIINCAPS;

typedef struct tagTIMECAPS {
    UINT wPeriodMin;
    UINT wPeriodMax;
} TIMECAPS;

typedef void (CALLBACK *LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

#define CALLBACK_NULL 0x0000
#define CALLBACK_FUNCTION 0x00030000
#define TIME_PERIODIC 0x0001
#define TIMERR_NOERROR 0
#define MMSYSERR_NOERROR 0
#define MMSYSERR_ERROR 1
#define MMSYSERR_NOMEM 7
#define MMSYSERR_ALLOCATED 4
#define MIDIERR_NODEVICE 64

#define MIM_OPEN 0x3C1
#define MIM_CLOSE 0x3C2
#define MIM_DATA 0x3C3
#define MIM_LONGDATA 0x3C4
#define MIM_ERROR 0x3C5
#define MIM_LONGERROR 0x3C6
#define MIM_MOREDATA 0x3CC

static inline int zt_midi_message_length(unsigned char status) {
    if (status < 0x80) {
        return 0;
    }
    if (status < 0xF0) {
        switch (status & 0xF0) {
            case 0xC0:
            case 0xD0:
                return 2;
            default:
                return 3;
        }
    }

    switch (status) {
        case 0xF1:
        case 0xF3:
            return 2;
        case 0xF2:
            return 3;
        case 0xF6:
        case 0xF8:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFE:
        case 0xFF:
            return 1;
        default:
            return 0;
    }
}

#if defined(__linux__)
typedef struct zt_alsa_midi_out_handle {
    snd_seq_t *seq;
    int src_port;
    int dest_client;
    int dest_port;
} zt_alsa_midi_out_handle;

static inline int zt_alsa_find_output_port_by_index(UINT index, int *client, int *port, char *name, size_t name_len) {
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return 0;
    }

    snd_seq_client_info_t *cinfo = NULL;
    snd_seq_port_info_t *pinfo = NULL;
    snd_seq_client_info_malloc(&cinfo);
    snd_seq_port_info_malloc(&pinfo);
    if (!cinfo || !pinfo) {
        if (cinfo) snd_seq_client_info_free(cinfo);
        if (pinfo) snd_seq_port_info_free(pinfo);
        snd_seq_close(seq);
        return 0;
    }

    UINT count = 0;
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        const int cl = snd_seq_client_info_get_client(cinfo);
        snd_seq_port_info_set_client(pinfo, cl);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            const unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            if ((caps & SND_SEQ_PORT_CAP_WRITE) == 0 || (caps & SND_SEQ_PORT_CAP_SUBS_WRITE) == 0) {
                continue;
            }
            if (count == index) {
                if (client) *client = cl;
                if (port) *port = snd_seq_port_info_get_port(pinfo);
                if (name && name_len > 0) {
                    const char *cn = snd_seq_client_info_get_name(cinfo);
                    const char *pn = snd_seq_port_info_get_name(pinfo);
                    snprintf(name, name_len, "%s:%s", cn ? cn : "ALSA", pn ? pn : "MIDI");
                    name[name_len - 1] = '\0';
                }
                snd_seq_client_info_free(cinfo);
                snd_seq_port_info_free(pinfo);
                snd_seq_close(seq);
                return 1;
            }
            count++;
        }
    }

    snd_seq_client_info_free(cinfo);
    snd_seq_port_info_free(pinfo);
    snd_seq_close(seq);
    return 0;
}

static inline UINT midiOutGetNumDevs(void) {
    UINT count = 0;
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return 0;
    }
    snd_seq_client_info_t *cinfo = NULL;
    snd_seq_port_info_t *pinfo = NULL;
    snd_seq_client_info_malloc(&cinfo);
    snd_seq_port_info_malloc(&pinfo);
    if (!cinfo || !pinfo) {
        if (cinfo) snd_seq_client_info_free(cinfo);
        if (pinfo) snd_seq_port_info_free(pinfo);
        snd_seq_close(seq);
        return 0;
    }

    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        const int cl = snd_seq_client_info_get_client(cinfo);
        snd_seq_port_info_set_client(pinfo, cl);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            const unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            if ((caps & SND_SEQ_PORT_CAP_WRITE) && (caps & SND_SEQ_PORT_CAP_SUBS_WRITE)) {
                count++;
            }
        }
    }

    snd_seq_client_info_free(cinfo);
    snd_seq_port_info_free(pinfo);
    snd_seq_close(seq);
    return count;
}
#elif !defined(__APPLE__)
static inline UINT midiOutGetNumDevs(void) { return 0; }
#endif

#if defined(__linux__)
static inline MMRESULT midiOutGetDevCaps(UINT dev, MIDIOUTCAPS *caps, UINT) {
    int client = -1;
    int port = -1;
    if (!caps) {
        return MMSYSERR_ERROR;
    }
    caps->szPname[0] = '\0';
    if (!zt_alsa_find_output_port_by_index(dev, &client, &port, caps->szPname, sizeof(caps->szPname))) {
        return MMSYSERR_ERROR;
    }
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutOpen(HMIDIOUT *h, UINT dev, DWORD_PTR, DWORD_PTR, DWORD) {
    int dest_client = -1;
    int dest_port = -1;
    if (h) {
        *h = 0;
    }
    if (!h || !zt_alsa_find_output_port_by_index(dev, &dest_client, &dest_port, NULL, 0)) {
        return MIDIERR_NODEVICE;
    }

    zt_alsa_midi_out_handle *ctx = (zt_alsa_midi_out_handle *)calloc(1, sizeof(zt_alsa_midi_out_handle));
    if (!ctx) {
        return MMSYSERR_NOMEM;
    }
    if (snd_seq_open(&ctx->seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        free(ctx);
        return MMSYSERR_ERROR;
    }
    snd_seq_set_client_name(ctx->seq, "zTracker");
    ctx->src_port = snd_seq_create_simple_port(
        ctx->seq,
        "zTracker Out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION
    );
    if (ctx->src_port < 0) {
        snd_seq_close(ctx->seq);
        free(ctx);
        return MMSYSERR_ERROR;
    }
    if (snd_seq_connect_to(ctx->seq, ctx->src_port, dest_client, dest_port) < 0) {
        snd_seq_close(ctx->seq);
        free(ctx);
        return MMSYSERR_ERROR;
    }
    ctx->dest_client = dest_client;
    ctx->dest_port = dest_port;
    *h = (HMIDIOUT)ctx;
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutClose(HMIDIOUT h) {
    zt_alsa_midi_out_handle *ctx = (zt_alsa_midi_out_handle *)h;
    if (!ctx) {
        return MMSYSERR_ERROR;
    }
    snd_seq_disconnect_to(ctx->seq, ctx->src_port, ctx->dest_client, ctx->dest_port);
    snd_seq_close(ctx->seq);
    free(ctx);
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutShortMsg(HMIDIOUT h, DWORD msg) {
    zt_alsa_midi_out_handle *ctx = (zt_alsa_midi_out_handle *)h;
    if (!ctx || !ctx->seq) {
        return MMSYSERR_ERROR;
    }

    const unsigned char status = (unsigned char)(msg & 0xFF);
    const unsigned char data1 = (unsigned char)((msg >> 8) & 0x7F);
    const unsigned char data2 = (unsigned char)((msg >> 16) & 0x7F);
    const unsigned char chan = (unsigned char)(status & 0x0F);
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, ctx->src_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    switch (status & 0xF0) {
        case 0x80: snd_seq_ev_set_noteoff(&ev, chan, data1, data2); break;
        case 0x90: snd_seq_ev_set_noteon(&ev, chan, data1, data2); break;
        case 0xA0: snd_seq_ev_set_keypress(&ev, chan, data1, data2); break;
        case 0xB0: snd_seq_ev_set_controller(&ev, chan, data1, data2); break;
        case 0xC0: snd_seq_ev_set_pgmchange(&ev, chan, data1); break;
        case 0xD0: snd_seq_ev_set_chanpress(&ev, chan, data1); break;
        case 0xE0: {
            const int bend = ((int)data1 | ((int)data2 << 7)) - 8192;
            snd_seq_ev_set_pitchbend(&ev, chan, bend);
            break;
        }
        default:
            if (status == 0xF8) ev.type = SND_SEQ_EVENT_CLOCK;
            else if (status == 0xFA) ev.type = SND_SEQ_EVENT_START;
            else if (status == 0xFB) ev.type = SND_SEQ_EVENT_CONTINUE;
            else if (status == 0xFC) ev.type = SND_SEQ_EVENT_STOP;
            else return MMSYSERR_NOERROR;
            break;
    }

    if (snd_seq_event_output_direct(ctx->seq, &ev) < 0) {
        return MMSYSERR_ERROR;
    }
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutReset(HMIDIOUT) { return MMSYSERR_NOERROR; }

// SysEx send (Linux / ALSA). bytes must include 0xF0 prefix and 0xF7
// terminator — the caller is responsible for framing. Length includes
// both bytes. Returns MMSYSERR_NOERROR on success.
static inline MMRESULT zt_midi_out_sysex(HMIDIOUT h, const unsigned char *bytes, int len) {
    zt_alsa_midi_out_handle *ctx = (zt_alsa_midi_out_handle *)h;
    if (!ctx || !ctx->seq || !bytes || len <= 0) return MMSYSERR_ERROR;
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, ctx->src_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_sysex(&ev, (unsigned int)len, (void *)bytes);
    if (snd_seq_event_output_direct(ctx->seq, &ev) < 0) return MMSYSERR_ERROR;
    return MMSYSERR_NOERROR;
}
#elif defined(__APPLE__)
typedef void (CALLBACK *zt_midi_in_callback_fn)(HMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

typedef struct zt_coremidi_out_handle {
    MIDIClientRef client;
    MIDIPortRef out_port;
    MIDIEndpointRef endpoint;
    // Display name captured at open() time, used to re-resolve a
    // stale endpoint reference when MIDISend fails. Virtual MIDI
    // ports (software synths, GBA emulators, IAC) commonly tear
    // down and recreate their destination when the host process
    // restarts or stops; without name-based recovery zTracker would
    // keep firing into a dead endpoint and the user would perceive
    // it as "MIDI Out lost connection".
    char endpoint_name[256];
} zt_coremidi_out_handle;

typedef struct zt_coremidi_in_handle {
    MIDIClientRef client;
    MIDIPortRef in_port;
    MIDIEndpointRef endpoint;
    zt_midi_in_callback_fn callback;
    DWORD_PTR instance;
    int started;
    unsigned char running_status;
    // SysEx assembly. When sysex_active != 0, every incoming byte is
    // appended to sysex_buf until 0xF7 closes the frame; then the whole
    // buffer (including the 0xF0 / 0xF7 framing) is pushed via
    // zt_sysex_inq_push. Cleared on overflow or when a new status byte
    // (other than realtime 0xF8..0xFF) interrupts the dump -- a
    // protocol violation we treat as "drop and resume."
    int sysex_active;
    int sysex_len;
    unsigned char sysex_buf[8192];   // matches ZT_SYSEX_MAX_LEN
} zt_coremidi_in_handle;

static inline int zt_coremidi_endpoint_name(MIDIEndpointRef endpoint, char *name, size_t name_len) {
    if (!name || name_len == 0 || endpoint == 0) {
        return 0;
    }
    name[0] = '\0';

    CFStringRef cf_name = NULL;
    OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &cf_name);
    if (err != noErr || !cf_name) {
        err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &cf_name);
    }
    if (err != noErr || !cf_name) {
        snprintf(name, name_len, "CoreMIDI");
        name[name_len - 1] = '\0';
        return 0;
    }
    if (!CFStringGetCString(cf_name, name, (CFIndex)name_len, kCFStringEncodingUTF8)) {
        snprintf(name, name_len, "CoreMIDI");
        name[name_len - 1] = '\0';
    }
    CFRelease(cf_name);
    return 1;
}

static inline UINT midiOutGetNumDevs(void) {
    return (UINT)MIDIGetNumberOfDestinations();
}

// Walk the live destination list and return the endpoint whose
// display name matches `target_name`. Used by midiOutShortMsg to
// recover from a stale endpoint after a virtual-port restart or
// USB re-plug. Returns 0 if no match (or empty target name).
static inline MIDIEndpointRef zt_coremidi_find_destination_by_name(const char *target_name) {
    if (!target_name || !target_name[0]) {
        return 0;
    }
    ItemCount n = MIDIGetNumberOfDestinations();
    char buf[256];
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef ep = MIDIGetDestination(i);
        if (ep == 0) continue;
        buf[0] = '\0';
        zt_coremidi_endpoint_name(ep, buf, sizeof(buf));
        if (strncmp(buf, target_name, sizeof(buf)) == 0) {
            return ep;
        }
    }
    return 0;
}

static inline MMRESULT midiOutGetDevCaps(UINT dev, MIDIOUTCAPS *caps, UINT) {
    if (!caps) {
        return MMSYSERR_ERROR;
    }
    caps->szPname[0] = '\0';
    MIDIEndpointRef endpoint = MIDIGetDestination((ItemCount)dev);
    if (endpoint == 0) {
        return MMSYSERR_ERROR;
    }
    zt_coremidi_endpoint_name(endpoint, caps->szPname, sizeof(caps->szPname));
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutOpen(HMIDIOUT *h, UINT dev, DWORD_PTR, DWORD_PTR, DWORD) {
    if (h) {
        *h = 0;
    }
    if (!h) {
        return MMSYSERR_ERROR;
    }

    MIDIEndpointRef endpoint = MIDIGetDestination((ItemCount)dev);
    if (endpoint == 0) {
        return MIDIERR_NODEVICE;
    }

    zt_coremidi_out_handle *ctx = (zt_coremidi_out_handle *)calloc(1, sizeof(zt_coremidi_out_handle));
    if (!ctx) {
        return MMSYSERR_NOMEM;
    }

    if (MIDIClientCreate(CFSTR("zTracker"), NULL, NULL, &ctx->client) != noErr) {
        free(ctx);
        return MMSYSERR_ERROR;
    }
    if (MIDIOutputPortCreate(ctx->client, CFSTR("zTracker Out"), &ctx->out_port) != noErr) {
        MIDIClientDispose(ctx->client);
        free(ctx);
        return MMSYSERR_ERROR;
    }

    ctx->endpoint = endpoint;
    // Cache the display name so midiOutShortMsg can re-resolve the
    // endpoint by name if the cached MIDIEndpointRef goes stale.
    ctx->endpoint_name[0] = '\0';
    zt_coremidi_endpoint_name(endpoint, ctx->endpoint_name, sizeof(ctx->endpoint_name));
    *h = (HMIDIOUT)ctx;
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutClose(HMIDIOUT h) {
    zt_coremidi_out_handle *ctx = (zt_coremidi_out_handle *)h;
    if (!ctx) {
        return MMSYSERR_ERROR;
    }
    if (ctx->out_port) {
        MIDIPortDispose(ctx->out_port);
    }
    if (ctx->client) {
        MIDIClientDispose(ctx->client);
    }
    free(ctx);
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiOutShortMsg(HMIDIOUT h, DWORD msg) {
    zt_coremidi_out_handle *ctx = (zt_coremidi_out_handle *)h;
    if (!ctx || !ctx->out_port) {
        return MMSYSERR_ERROR;
    }

    const unsigned char status = (unsigned char)(msg & 0xFF);
    const unsigned char data1 = (unsigned char)((msg >> 8) & 0x7F);
    const unsigned char data2 = (unsigned char)((msg >> 16) & 0x7F);
    const int len = zt_midi_message_length(status);
    if (len <= 0) {
        return MMSYSERR_NOERROR;
    }

    unsigned char bytes[3];
    bytes[0] = status;
    bytes[1] = data1;
    bytes[2] = data2;

    MIDIPacketList packet_list;
    MIDIPacket *packet = MIDIPacketListInit(&packet_list);
    packet = MIDIPacketListAdd(&packet_list, sizeof(packet_list), packet, 0, (UInt16)len, bytes);
    if (!packet) {
        return MMSYSERR_ERROR;
    }

    // First attempt with the cached endpoint. Fast path -- a healthy
    // device hits this and returns immediately.
    OSStatus result = noErr;
    if (ctx->endpoint != 0) {
        result = MIDISend(ctx->out_port, ctx->endpoint, &packet_list);
        if (result == noErr) {
            return MMSYSERR_NOERROR;
        }
    }

    // Recovery path: the endpoint either was 0 to begin with (the
    // device disappeared between open and now) or MIDISend errored.
    // Re-resolve by display name -- handles virtual-port restarts
    // (software synth quit + relaunch, IAC bus reload), USB unplug
    // and replug, and the macOS "MIDI device sleep" cycle. Caps the
    // recovery cost at one MIDIGetNumberOfDestinations walk plus one
    // retry per failing send.
    if (ctx->endpoint_name[0] != '\0') {
        MIDIEndpointRef fresh = zt_coremidi_find_destination_by_name(ctx->endpoint_name);
        if (fresh != 0 && fresh != ctx->endpoint) {
            ctx->endpoint = fresh;
            result = MIDISend(ctx->out_port, ctx->endpoint, &packet_list);
            if (result == noErr) {
                return MMSYSERR_NOERROR;
            }
        }
    }

    return MMSYSERR_ERROR;
}

static inline MMRESULT midiOutReset(HMIDIOUT) { return MMSYSERR_NOERROR; }

// SysEx send (macOS / CoreMIDI). bytes must include 0xF0 prefix and
// 0xF7 terminator. Length includes both bytes. CoreMIDI packets are
// limited to 256 bytes per packet, but a single packet list can hold
// multiple packets — for SysEx longer than 256 bytes we segment into
// multiple packets within one packet list (timestamp 0 = "now"). The
// receiver reassembles by spec since 0xF0..0xF7 framing is preserved.
//
// The packet-list buffer is heap-allocated (audit M6). Previously it
// was a 17 KB stack array which is fine on default-stack threads
// (>= 512 KB) but risky on small-stack worker threads. Caller is
// responsible for splitting > 256 KB dumps; we cap at 256 KB to bound
// the malloc.
static inline MMRESULT zt_midi_out_sysex(HMIDIOUT h, const unsigned char *bytes, int len) {
    zt_coremidi_out_handle *ctx = (zt_coremidi_out_handle *)h;
    if (!ctx || !ctx->out_port || !bytes || len <= 0) return MMSYSERR_ERROR;

    enum { MAX_SYSEX = 256 * 1024, CHUNK = 256 };
    if (len > MAX_SYSEX) return MMSYSERR_ERROR;
    // Storage: packet-list overhead is per-packet (header + payload).
    // Reserve len + (len/CHUNK + 1)*32 for headers, plus base list
    // header. 64 KB safety margin covers any version-skew metadata.
    size_t buf_sz = (size_t)len + ((size_t)len / CHUNK + 1) * 64 + 1024;
    Byte *buf = (Byte *)malloc(buf_sz);
    if (!buf) return MMSYSERR_ERROR;
    MIDIPacketList *pl = (MIDIPacketList *)buf;
    MIDIPacket *pkt = MIDIPacketListInit(pl);

    int offset = 0;
    while (offset < len) {
        int chunk = len - offset;
        if (chunk > CHUNK) chunk = CHUNK;
        pkt = MIDIPacketListAdd(pl, buf_sz, pkt, 0,
                                (UInt16)chunk, bytes + offset);
        if (!pkt) { free(buf); return MMSYSERR_ERROR; }
        offset += chunk;
    }

    // First attempt with the cached endpoint.
    if (ctx->endpoint != 0) {
        if (MIDISend(ctx->out_port, ctx->endpoint, pl) == noErr) {
            free(buf);
            return MMSYSERR_NOERROR;
        }
    }
    // Recovery: re-resolve endpoint by name (same pattern as the short
    // message path) and retry once before giving up.
    if (ctx->endpoint_name[0] != '\0') {
        MIDIEndpointRef fresh = zt_coremidi_find_destination_by_name(ctx->endpoint_name);
        if (fresh != 0 && fresh != ctx->endpoint) {
            ctx->endpoint = fresh;
            if (MIDISend(ctx->out_port, ctx->endpoint, pl) == noErr) {
                free(buf);
                return MMSYSERR_NOERROR;
            }
        }
    }
    free(buf);
    return MMSYSERR_ERROR;
}

static inline void zt_coremidi_emit_data(zt_coremidi_in_handle *ctx, unsigned int packed_msg) {
    if (!ctx || !ctx->callback) {
        return;
    }
    ctx->callback((HMIDIIN)ctx, MIM_DATA, ctx->instance, (DWORD_PTR)packed_msg, 0);
}

// Forward declaration of the SysEx queue producer; defined in
// src/sysex_inq.cpp. Declared inline here so the CoreMIDI callback
// (which lives in this header) can push without dragging the queue
// header into every translation unit that includes winmm_compat.h.
extern "C" int zt_sysex_inq_push(const unsigned char *bytes, int len);

static inline void zt_coremidi_read_proc(const MIDIPacketList *packet_list, void *read_proc_ref_con, void *) {
    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)read_proc_ref_con;
    if (!ctx || !packet_list) {
        return;
    }

    const MIDIPacket *packet = &packet_list->packet[0];
    for (UInt32 p = 0; p < packet_list->numPackets; ++p) {
        const unsigned char *data = packet->data;
        UInt16 i = 0;
        while (i < packet->length) {
            unsigned char b = data[i];

            // SysEx accumulator. F0 starts a frame; every byte (including
            // realtime bytes 0xF8..0xFF, which the spec allows mid-SysEx
            // -- we leave them in the buffer rather than splitting) is
            // appended until F7 closes it. On overflow we drop the frame
            // and reset; on a non-F7 status byte interrupting the dump
            // we drop too (protocol violation).
            if (ctx->sysex_active) {
                if ((int)ctx->sysex_len < (int)sizeof(ctx->sysex_buf)) {
                    ctx->sysex_buf[ctx->sysex_len++] = b;
                }
                if (b == 0xF7) {
                    if ((int)ctx->sysex_len <= (int)sizeof(ctx->sysex_buf)) {
                        zt_sysex_inq_push(ctx->sysex_buf, ctx->sysex_len);
                    }
                    ctx->sysex_active = 0;
                    ctx->sysex_len = 0;
                    i++;
                    continue;
                }
                if ((b & 0x80U) != 0U && b < 0xF8) {
                    // Non-realtime status byte mid-SysEx -- abort.
                    ctx->sysex_active = 0;
                    ctx->sysex_len = 0;
                    // Fall through so this byte gets handled normally below.
                } else {
                    i++;
                    continue;
                }
            }

            if (b == 0xF0) {
                ctx->sysex_active = 1;
                ctx->sysex_len = 0;
                ctx->sysex_buf[ctx->sysex_len++] = b;
                ctx->running_status = 0;
                i++;
                continue;
            }

            if ((b & 0x80U) != 0U) {
                if (b < 0xF0) {
                    ctx->running_status = b;
                } else if (b != 0xF7) {
                    ctx->running_status = 0;
                }

                int len = zt_midi_message_length(b);
                if (len <= 0 || i + (UInt16)len > packet->length) {
                    i++;
                    continue;
                }
                unsigned int msg = (unsigned int)b;
                if (len > 1) {
                    msg |= ((unsigned int)(data[i + 1] & 0x7F)) << 8;
                }
                if (len > 2) {
                    msg |= ((unsigned int)(data[i + 2] & 0x7F)) << 16;
                }
                zt_coremidi_emit_data(ctx, msg);
                i = (UInt16)(i + len);
                continue;
            }

            if (ctx->running_status != 0) {
                int len = zt_midi_message_length(ctx->running_status);
                if (len == 2 && i < packet->length) {
                    unsigned int msg = (unsigned int)ctx->running_status |
                                       ((unsigned int)(data[i] & 0x7F) << 8);
                    zt_coremidi_emit_data(ctx, msg);
                    i++;
                    continue;
                }
                if (len == 3 && i + 1 < packet->length) {
                    unsigned int msg = (unsigned int)ctx->running_status |
                                       ((unsigned int)(data[i] & 0x7F) << 8) |
                                       ((unsigned int)(data[i + 1] & 0x7F) << 16);
                    zt_coremidi_emit_data(ctx, msg);
                    i += 2;
                    continue;
                }
            }
            i++;
        }
        packet = MIDIPacketNext(packet);
    }
}

static inline UINT midiInGetNumDevs(void) {
    return (UINT)MIDIGetNumberOfSources();
}

static inline MMRESULT midiInGetDevCaps(UINT dev, MIDIINCAPS *caps, UINT) {
    if (!caps) {
        return MMSYSERR_ERROR;
    }
    caps->szPname[0] = '\0';
    MIDIEndpointRef endpoint = MIDIGetSource((ItemCount)dev);
    if (endpoint == 0) {
        return MMSYSERR_ERROR;
    }
    zt_coremidi_endpoint_name(endpoint, caps->szPname, sizeof(caps->szPname));
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInOpen(HMIDIIN *h, UINT dev, DWORD_PTR callback, DWORD_PTR instance, DWORD flags) {
    if (h) {
        *h = 0;
    }
    if (!h) {
        return MMSYSERR_ERROR;
    }
    if ((flags & CALLBACK_FUNCTION) == 0) {
        return MMSYSERR_ERROR;
    }

    MIDIEndpointRef endpoint = MIDIGetSource((ItemCount)dev);
    if (endpoint == 0) {
        return MIDIERR_NODEVICE;
    }

    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)calloc(1, sizeof(zt_coremidi_in_handle));
    if (!ctx) {
        return MMSYSERR_NOMEM;
    }

    if (MIDIClientCreate(CFSTR("zTracker In"), NULL, NULL, &ctx->client) != noErr) {
        free(ctx);
        return MMSYSERR_ERROR;
    }
    if (MIDIInputPortCreate(ctx->client, CFSTR("zTracker Input"), zt_coremidi_read_proc, ctx, &ctx->in_port) != noErr) {
        MIDIClientDispose(ctx->client);
        free(ctx);
        return MMSYSERR_ERROR;
    }

    ctx->endpoint = endpoint;
    ctx->callback = (zt_midi_in_callback_fn)callback;
    ctx->instance = instance;
    ctx->started = 0;
    ctx->running_status = 0;
    *h = (HMIDIIN)ctx;
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInClose(HMIDIIN h) {
    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)h;
    if (!ctx) {
        return MMSYSERR_ERROR;
    }

    if (ctx->started && ctx->in_port && ctx->endpoint) {
        (void)MIDIPortDisconnectSource(ctx->in_port, ctx->endpoint);
        ctx->started = 0;
    }
    if (ctx->callback) {
        ctx->callback(h, MIM_CLOSE, ctx->instance, 0, 0);
    }
    if (ctx->in_port) {
        MIDIPortDispose(ctx->in_port);
    }
    if (ctx->client) {
        MIDIClientDispose(ctx->client);
    }
    free(ctx);
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInPrepareHeader(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_NOERROR; }
static inline MMRESULT midiInUnprepareHeader(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_NOERROR; }
static inline MMRESULT midiInAddBuffer(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_NOERROR; }

static inline MMRESULT midiInStart(HMIDIIN h) {
    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)h;
    if (!ctx || !ctx->in_port || !ctx->endpoint) {
        return MMSYSERR_ERROR;
    }
    if (!ctx->started) {
        if (MIDIPortConnectSource(ctx->in_port, ctx->endpoint, NULL) != noErr) {
            return MMSYSERR_ERROR;
        }
        ctx->started = 1;
        if (ctx->callback) {
            ctx->callback(h, MIM_OPEN, ctx->instance, 0, 0);
        }
    }
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInStop(HMIDIIN h) {
    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)h;
    if (!ctx) {
        return MMSYSERR_ERROR;
    }
    if (ctx->started && ctx->in_port && ctx->endpoint) {
        if (MIDIPortDisconnectSource(ctx->in_port, ctx->endpoint) != noErr) {
            return MMSYSERR_ERROR;
        }
        ctx->started = 0;
    }
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInReset(HMIDIIN h) {
    zt_coremidi_in_handle *ctx = (zt_coremidi_in_handle *)h;
    if (!ctx) {
        return MMSYSERR_ERROR;
    }
    ctx->running_status = 0;
    return MMSYSERR_NOERROR;
}

static inline MMRESULT midiInGetErrorText(MMRESULT, char *buffer, UINT size) {
    if (buffer && size > 0) {
        snprintf(buffer, size, "CoreMIDI error");
        buffer[size - 1] = '\0';
    }
    return MMSYSERR_NOERROR;
}
#else
static inline MMRESULT midiOutGetDevCaps(UINT, MIDIOUTCAPS *caps, UINT) {
    if (caps) {
        caps->szPname[0] = '\0';
    }
    return MMSYSERR_ERROR;
}
static inline MMRESULT midiOutOpen(HMIDIOUT *h, UINT, DWORD_PTR, DWORD_PTR, DWORD) {
    if (h) {
        *h = 0;
    }
    return MMSYSERR_ERROR;
}
static inline MMRESULT midiOutClose(HMIDIOUT) { return MMSYSERR_ERROR; }
static inline MMRESULT midiOutShortMsg(HMIDIOUT, DWORD) { return MMSYSERR_ERROR; }
static inline MMRESULT midiOutReset(HMIDIOUT) { return MMSYSERR_ERROR; }
// SysEx send fallback (no-op on platforms without a real backend).
static inline MMRESULT zt_midi_out_sysex(HMIDIOUT, const unsigned char *, int) {
    return MMSYSERR_ERROR;
}
#endif

#if !defined(__APPLE__)
static inline UINT midiInGetNumDevs(void) { return 0; }
static inline MMRESULT midiInGetDevCaps(UINT, MIDIINCAPS *caps, UINT) {
    if (caps) {
        caps->szPname[0] = '\0';
    }
    return MMSYSERR_ERROR;
}
static inline MMRESULT midiInOpen(HMIDIIN *h, UINT, DWORD_PTR, DWORD_PTR, DWORD) {
    if (h) {
        *h = 0;
    }
    return MMSYSERR_ERROR;
}
static inline MMRESULT midiInClose(HMIDIIN) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInPrepareHeader(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInUnprepareHeader(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInAddBuffer(HMIDIIN, MIDIHDR *, UINT) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInStart(HMIDIIN) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInStop(HMIDIIN) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInReset(HMIDIIN) { return MMSYSERR_ERROR; }
static inline MMRESULT midiInGetErrorText(MMRESULT, char *buffer, UINT size) {
    if (buffer && size > 0) {
        buffer[0] = '\0';
    }
    return MMSYSERR_ERROR;
}
#endif

static inline MMRESULT timeGetDevCaps(TIMECAPS *tc, UINT) {
    if (tc) {
        tc->wPeriodMin = 1;
        tc->wPeriodMax = 16;
    }
    return TIMERR_NOERROR;
}
static inline MMRESULT timeBeginPeriod(UINT) { return TIMERR_NOERROR; }
static inline MMRESULT timeEndPeriod(UINT) { return TIMERR_NOERROR; }
static inline UINT timeSetEvent(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT) { return 1; }
static inline MMRESULT timeKillEvent(UINT) { return TIMERR_NOERROR; }

#endif

#endif
