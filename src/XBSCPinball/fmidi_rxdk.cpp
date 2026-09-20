/*
 * RXDK-friendly Standard MIDI implementation with the fmidi API surface used
 * by XBSCPinball.  No Windows API, TLS, fmt, filesystem, or modern CRT calls.
 */

#include "pch.h"
#include "fmidi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>

struct XbFmidiOwnedEvent
{
    fmidi_event_t* evt;
    uint32_t tick;
    uint16_t track;
    uint32_t order;

    XbFmidiOwnedEvent()
        : evt(NULL), tick(0), track(0), order(0)
    {
    }
};

struct XbFmidiTrack
{
    std::vector<XbFmidiOwnedEvent*> events;
};

struct fmidi_smf
{
    fmidi_smf_info_t info;
    std::vector<XbFmidiTrack> tracks;
    std::vector<XbFmidiOwnedEvent*> owned;
};

struct XbFmidiScheduledEvent
{
    double time;
    uint16_t track;
    uint32_t order;
    const fmidi_event_t* evt;
};

struct fmidi_seq
{
    std::vector<XbFmidiScheduledEvent> events;
    size_t index;

    fmidi_seq() : index(0) {}
};

struct fmidi_player
{
    fmidi_seq_t* seq;
    double timepos;
    double speed;
    int running;

    void (*event_cb)(const fmidi_event_t*, void*);
    void* event_ud;

    void (*finish_cb)(void*);
    void* finish_ud;

    fmidi_player()
        : seq(NULL), timepos(0.0), speed(1.0), running(0),
        event_cb(NULL), event_ud(NULL), finish_cb(NULL), finish_ud(NULL)
    {
    }
};

static fmidi_error_info_t g_fmidi_error = { fmidi_ok };

static void SetError(fmidi_status_t status)
{
    g_fmidi_error.code = status;
}

static uint16_t ReadBE16(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t ReadBE32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) |
        ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) |
        (uint32_t)p[3];
}

static int ReadVLQ(
    const uint8_t* data,
    size_t end,
    size_t* pos,
    uint32_t* value)
{
    uint32_t v = 0;
    int count = 0;

    while (*pos < end && count < 4)
    {
        uint8_t b = data[(*pos)++];
        v = (v << 7) | (uint32_t)(b & 0x7F);
        ++count;

        if ((b & 0x80) == 0)
        {
            *value = v;
            return 1;
        }
    }

    return 0;
}

static int MidiMessageLength(uint8_t status)
{
    if (status >= 0x80 && status <= 0xEF)
    {
        const int hi = status >> 4;
        return (hi == 0xC || hi == 0xD) ? 2 : 3;
    }

    switch (status)
    {
    case 0xF1: return 2;
    case 0xF2: return 3;
    case 0xF3: return 2;
    case 0xF6: return 1;
    case 0xF8: return 1;
    case 0xF9: return 1;
    case 0xFA: return 1;
    case 0xFB: return 1;
    case 0xFC: return 1;
    case 0xFD: return 1;
    case 0xFE: return 1;
    default:   return 0;
    }
}

static fmidi_event_t* AllocEvent(
    fmidi_event_type_t type,
    uint32_t delta,
    const uint8_t* payload,
    uint32_t payloadLen)
{
    const size_t base = offsetof(fmidi_event_t, data);
    const size_t bytes = base + (payloadLen ? payloadLen : 1);

    uint8_t* raw = new uint8_t[bytes];
    if (!raw)
        return NULL;

    fmidi_event_t* evt = (fmidi_event_t*)raw;
    evt->type = type;
    evt->delta = delta;
    evt->datalen = payloadLen;

    if (payloadLen && payload)
        memcpy(evt->data, payload, payloadLen);
    else
        evt->data[0] = 0;

    return evt;
}

static int AddOwnedEvent(
    fmidi_smf_t* smf,
    uint16_t trackIndex,
    uint32_t tick,
    uint32_t order,
    fmidi_event_t* evt)
{
    if (!smf || !evt || trackIndex >= smf->tracks.size())
        return 0;

    XbFmidiOwnedEvent* owned = new XbFmidiOwnedEvent();
    if (!owned)
    {
        delete[](uint8_t*)evt;
        return 0;
    }

    owned->evt = evt;
    owned->tick = tick;
    owned->track = trackIndex;
    owned->order = order;

    smf->owned.push_back(owned);
    smf->tracks[trackIndex].events.push_back(owned);
    return 1;
}

static int ParseTrack(
    fmidi_smf_t* smf,
    uint16_t trackIndex,
    const uint8_t* data,
    size_t start,
    size_t end)
{
    size_t pos = start;
    uint8_t runningStatus = 0;
    uint32_t absoluteTick = 0;
    uint32_t order = 0;

    while (pos < end)
    {
        uint32_t delta = 0;
        if (!ReadVLQ(data, end, &pos, &delta))
        {
            SetError(fmidi_err_format);
            return 0;
        }

        absoluteTick += delta;

        if (pos >= end)
        {
            SetError(fmidi_err_eof);
            return 0;
        }

        uint8_t status = data[pos++];
        int running = 0;

        if (status < 0x80)
        {
            if (!runningStatus)
            {
                SetError(fmidi_err_format);
                return 0;
            }

            running = 1;
            --pos;
            status = runningStatus;
        }
        else if (status < 0xF0)
        {
            runningStatus = status;
        }

        if (status == 0xFF)
        {
            if (pos >= end)
            {
                SetError(fmidi_err_eof);
                return 0;
            }

            const uint8_t metaType = data[pos++];
            uint32_t len = 0;
            if (!ReadVLQ(data, end, &pos, &len) || len > end - pos)
            {
                SetError(fmidi_err_format);
                return 0;
            }

            std::vector<uint8_t> payload;
            payload.resize((size_t)len + 1);
            payload[0] = metaType;
            if (len)
                memcpy(&payload[1], data + pos, len);
            pos += len;

            fmidi_event_t* evt = AllocEvent(
                fmidi_event_meta,
                delta,
                &payload[0],
                (uint32_t)payload.size());

            if (!evt || !AddOwnedEvent(smf, trackIndex, absoluteTick, order++, evt))
            {
                SetError(fmidi_err_input);
                return 0;
            }

            if (metaType == 0x2F)
                break;

            continue;
        }

        if (status == 0xF0 || status == 0xF7)
        {
            uint32_t len = 0;
            if (!ReadVLQ(data, end, &pos, &len) || len > end - pos)
            {
                SetError(fmidi_err_format);
                return 0;
            }

            std::vector<uint8_t> payload;
            payload.resize((size_t)len + 1);
            payload[0] = status;
            if (len)
                memcpy(&payload[1], data + pos, len);
            pos += len;

            fmidi_event_t* evt = AllocEvent(
                fmidi_event_escape,
                delta,
                &payload[0],
                (uint32_t)payload.size());

            if (!evt || !AddOwnedEvent(smf, trackIndex, absoluteTick, order++, evt))
            {
                SetError(fmidi_err_input);
                return 0;
            }

            runningStatus = 0;
            continue;
        }

        const int msgLen = MidiMessageLength(status);
        if (msgLen <= 0)
        {
            SetError(fmidi_err_format);
            return 0;
        }

        uint8_t msg[3];
        msg[0] = status;

        int requiredData = msgLen - 1;
        int copied = 0;

        if (running)
        {
            if (pos >= end)
            {
                SetError(fmidi_err_eof);
                return 0;
            }
            msg[1] = data[pos++];
            copied = 1;
        }

        while (copied < requiredData)
        {
            if (pos >= end)
            {
                SetError(fmidi_err_eof);
                return 0;
            }
            msg[1 + copied] = data[pos++];
            ++copied;
        }

        fmidi_event_t* evt = AllocEvent(
            fmidi_event_message,
            delta,
            msg,
            (uint32_t)msgLen);

        if (!evt || !AddOwnedEvent(smf, trackIndex, absoluteTick, order++, evt))
        {
            SetError(fmidi_err_input);
            return 0;
        }
    }

    return 1;
}

static double TickDeltaToSeconds(
    uint32_t ticks,
    uint16_t division,
    uint32_t tempoUsec)
{
    if (division & 0x8000)
    {
        int fps = -(int8_t)(division >> 8);
        int ticksPerFrame = division & 0xFF;
        if (fps <= 0 || ticksPerFrame <= 0)
            return 0.0;
        return (double)ticks / ((double)fps * (double)ticksPerFrame);
    }

    if (!division)
        return 0.0;

    return ((double)ticks * (double)tempoUsec) /
        ((double)division * 1000000.0);
}

static bool ScheduleLess(
    const XbFmidiOwnedEvent* a,
    const XbFmidiOwnedEvent* b)
{
    if (a->tick != b->tick)
        return a->tick < b->tick;
    if (a->track != b->track)
        return a->track < b->track;
    return a->order < b->order;
}

extern "C" {

    fmidi_status_t fmidi_errno(void)
    {
        return g_fmidi_error.code;
    }

    const fmidi_error_info_t* fmidi_errinfo(void)
    {
        return &g_fmidi_error;
    }

    const char* fmidi_strerror(fmidi_status_t status)
    {
        switch (status)
        {
        case fmidi_ok:            return "success";
        case fmidi_err_format:    return "invalid MIDI format";
        case fmidi_err_eof:       return "unexpected end of MIDI file";
        case fmidi_err_input:     return "MIDI input error";
        case fmidi_err_largefile: return "MIDI file too large";
        case fmidi_err_output:    return "MIDI output error";
        default:                  return "unknown MIDI error";
        }
    }

    fmidi_smf_t* fmidi_smf_mem_read(const uint8_t* data, size_t length)
    {
        SetError(fmidi_ok);

        if (!data || length < 14 || memcmp(data, "MThd", 4) != 0)
        {
            SetError(fmidi_err_format);
            return NULL;
        }

        const uint32_t headerLength = ReadBE32(data + 4);
        if (headerLength < 6 || 8u + headerLength > length)
        {
            SetError(fmidi_err_format);
            return NULL;
        }

        const uint16_t format = ReadBE16(data + 8);
        const uint16_t trackCount = ReadBE16(data + 10);
        const uint16_t division = ReadBE16(data + 12);

        if (trackCount == 0)
        {
            SetError(fmidi_err_format);
            return NULL;
        }

        fmidi_smf_t* smf = new fmidi_smf_t();
        if (!smf)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        smf->info.format = format;
        smf->info.track_count = trackCount;
        smf->info.delta_unit = division;
        smf->tracks.resize(trackCount);

        size_t pos = 8u + headerLength;

        for (uint16_t t = 0; t < trackCount; ++t)
        {
            if (pos + 8 > length || memcmp(data + pos, "MTrk", 4) != 0)
            {
                fmidi_smf_free(smf);
                SetError(fmidi_err_format);
                return NULL;
            }

            const uint32_t trackLength = ReadBE32(data + pos + 4);
            pos += 8;

            if ((size_t)trackLength > length - pos)
            {
                fmidi_smf_free(smf);
                SetError(fmidi_err_eof);
                return NULL;
            }

            const size_t end = pos + trackLength;
            if (!ParseTrack(smf, t, data, pos, end))
            {
                fmidi_smf_free(smf);
                return NULL;
            }

            pos = end;
        }

        return smf;
    }

    fmidi_smf_t* fmidi_smf_stream_read(FILE* stream)
    {
        if (!stream)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        long original = ftell(stream);
        if (original < 0)
            original = 0;

        if (fseek(stream, 0, SEEK_END) != 0)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        long length = ftell(stream);
        if (length <= 0 || length > (64L * 1024L * 1024L))
        {
            SetError(length > 0 ? fmidi_err_largefile : fmidi_err_input);
            return NULL;
        }

        if (fseek(stream, 0, SEEK_SET) != 0)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        std::vector<uint8_t> data;
        data.resize((size_t)length);

        if (fread(&data[0], 1, (size_t)length, stream) != (size_t)length)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        if (original > 0)
            fseek(stream, original, SEEK_SET);

        return fmidi_smf_mem_read(&data[0], data.size());
    }

    fmidi_smf_t* fmidi_smf_file_read(const char* filename)
    {
        if (!filename)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        FILE* f = fopen(filename, "rb");
        if (!f)
        {
            SetError(fmidi_err_input);
            return NULL;
        }

        fmidi_smf_t* smf = fmidi_smf_stream_read(f);
        fclose(f);
        return smf;
    }

    void fmidi_smf_free(fmidi_smf_t* smf)
    {
        if (!smf)
            return;

        for (size_t i = 0; i < smf->owned.size(); ++i)
        {
            XbFmidiOwnedEvent* owned = smf->owned[i];
            if (owned)
            {
                delete[](uint8_t*)owned->evt;
                delete owned;
            }
        }

        delete smf;
    }

    const fmidi_smf_info_t* fmidi_smf_get_info(const fmidi_smf_t* smf)
    {
        return smf ? &smf->info : NULL;
    }

    void fmidi_smf_track_begin(fmidi_track_iter_t* it, uint16_t track)
    {
        if (!it)
            return;
        it->track = track;
        it->index = 0;
    }

    const fmidi_event_t* fmidi_smf_track_next(
        const fmidi_smf_t* smf,
        fmidi_track_iter_t* it)
    {
        if (!smf || !it || it->track >= smf->tracks.size())
            return NULL;

        const XbFmidiTrack& track = smf->tracks[it->track];
        if (it->index >= track.events.size())
            return NULL;

        return track.events[it->index++]->evt;
    }

    fmidi_seq_t* fmidi_seq_new(const fmidi_smf_t* smf)
    {
        if (!smf)
            return NULL;

        fmidi_seq_t* seq = new fmidi_seq_t();
        if (!seq)
            return NULL;

        std::vector<XbFmidiOwnedEvent*> merged = smf->owned;
        std::sort(merged.begin(), merged.end(), ScheduleLess);

        uint32_t previousTick = 0;
        uint32_t tempo = 500000;
        double timeSeconds = 0.0;

        seq->events.reserve(merged.size());

        for (size_t i = 0; i < merged.size(); ++i)
        {
            XbFmidiOwnedEvent* owned = merged[i];

            if (owned->tick > previousTick)
            {
                timeSeconds += TickDeltaToSeconds(
                    owned->tick - previousTick,
                    smf->info.delta_unit,
                    tempo);
                previousTick = owned->tick;
            }

            XbFmidiScheduledEvent scheduled;
            scheduled.time = timeSeconds;
            scheduled.track = owned->track;
            scheduled.order = owned->order;
            scheduled.evt = owned->evt;
            seq->events.push_back(scheduled);

            const fmidi_event_t* evt = owned->evt;
            if (evt && evt->type == fmidi_event_meta &&
                evt->datalen >= 4 && evt->data[0] == 0x51)
            {
                tempo = ((uint32_t)evt->data[1] << 16) |
                    ((uint32_t)evt->data[2] << 8) |
                    (uint32_t)evt->data[3];
                if (!tempo)
                    tempo = 500000;
            }
        }

        seq->index = 0;
        return seq;
    }

    void fmidi_seq_free(fmidi_seq_t* seq)
    {
        delete seq;
    }

    void fmidi_seq_rewind(fmidi_seq_t* seq)
    {
        if (seq)
            seq->index = 0;
    }

    int fmidi_seq_peek_event(fmidi_seq_t* seq, fmidi_seq_event_t* evt)
    {
        if (!seq || seq->index >= seq->events.size())
            return 0;

        if (evt)
        {
            const XbFmidiScheduledEvent& src = seq->events[seq->index];
            evt->time = src.time;
            evt->track = src.track;
            evt->event = src.evt;
        }

        return 1;
    }

    int fmidi_seq_next_event(fmidi_seq_t* seq, fmidi_seq_event_t* evt)
    {
        if (!fmidi_seq_peek_event(seq, evt))
            return 0;

        ++seq->index;
        return 1;
    }

    double fmidi_smf_compute_duration(const fmidi_smf_t* smf)
    {
        fmidi_seq_t* seq = fmidi_seq_new(smf);
        if (!seq)
            return 0.0;

        double duration = 0.0;
        if (!seq->events.empty())
            duration = seq->events.back().time;

        fmidi_seq_free(seq);
        return duration;
    }

    fmidi_player_t* fmidi_player_new(fmidi_smf_t* smf)
    {
        if (!smf)
            return NULL;

        fmidi_player_t* player = new fmidi_player_t();
        if (!player)
            return NULL;

        player->seq = fmidi_seq_new(smf);
        if (!player->seq)
        {
            delete player;
            return NULL;
        }

        return player;
    }

    void fmidi_player_tick(fmidi_player_t* player, double delta)
    {
        if (!player || !player->running || !player->seq)
            return;

        if (delta < 0.0)
            delta = 0.0;

        player->timepos += delta * player->speed;

        fmidi_seq_event_t evt;
        while (fmidi_seq_peek_event(player->seq, &evt) &&
            evt.time <= player->timepos)
        {
            fmidi_seq_next_event(player->seq, &evt);

            if (player->event_cb && evt.event)
                player->event_cb(evt.event, player->event_ud);
        }

        if (!fmidi_seq_peek_event(player->seq, NULL))
        {
            player->running = 0;
            if (player->finish_cb)
                player->finish_cb(player->finish_ud);
        }
    }

    void fmidi_player_free(fmidi_player_t* player)
    {
        if (!player)
            return;
        fmidi_seq_free(player->seq);
        delete player;
    }

    void fmidi_player_start(fmidi_player_t* player)
    {
        if (player)
            player->running = 1;
    }

    void fmidi_player_stop(fmidi_player_t* player)
    {
        if (player)
            player->running = 0;
    }

    void fmidi_player_rewind(fmidi_player_t* player)
    {
        if (!player)
            return;
        if (player->seq)
            fmidi_seq_rewind(player->seq);
        player->timepos = 0.0;
    }

    int fmidi_player_running(const fmidi_player_t* player)
    {
        return player ? player->running : 0;
    }

    double fmidi_player_current_time(const fmidi_player_t* player)
    {
        return player ? player->timepos : 0.0;
    }

    void fmidi_player_goto_time(fmidi_player_t* player, double time)
    {
        if (!player || !player->seq)
            return;

        if (time < 0.0)
            time = 0.0;

        fmidi_seq_rewind(player->seq);
        player->timepos = time;

        fmidi_seq_event_t evt;
        while (fmidi_seq_peek_event(player->seq, &evt) && evt.time < time)
            fmidi_seq_next_event(player->seq, NULL);
    }

    double fmidi_player_current_speed(const fmidi_player_t* player)
    {
        return player ? player->speed : 1.0;
    }

    void fmidi_player_set_speed(fmidi_player_t* player, double speed)
    {
        if (player)
            player->speed = speed;
    }

    void fmidi_player_event_callback(
        fmidi_player_t* player,
        void (*callback)(const fmidi_event_t*, void*),
        void* userdata)
    {
        if (!player)
            return;
        player->event_cb = callback;
        player->event_ud = userdata;
    }

    void fmidi_player_finish_callback(
        fmidi_player_t* player,
        void (*callback)(void*),
        void* userdata)
    {
        if (!player)
            return;
        player->finish_cb = callback;
        player->finish_ud = userdata;
    }

} /* extern "C" */
