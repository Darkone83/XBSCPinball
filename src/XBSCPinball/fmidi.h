#pragma once

/*
 * Minimal fmidi-compatible API used by XBSCPinball.
 *
 * This header intentionally exposes only the SMF/sequencer/player surface
 * required by midi_xbox.cpp.  It avoids the upstream C++ helper layer and
 * fmt dependency, which are unnecessary on RXDK.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct fmidi_smf fmidi_smf_t;
    typedef struct fmidi_seq fmidi_seq_t;
    typedef struct fmidi_player fmidi_player_t;

    typedef enum fmidi_event_type
    {
        fmidi_event_meta = 1,
        fmidi_event_message = 2,
        fmidi_event_escape = 3,
        fmidi_event_xmi_timbre = 4,
        fmidi_event_xmi_branch_point = 5
    } fmidi_event_type_t;

    typedef struct fmidi_event
    {
        fmidi_event_type_t type;
        uint32_t delta;
        uint32_t datalen;
        uint8_t data[1];
    } fmidi_event_t;

    typedef struct fmidi_smf_info
    {
        uint16_t format;
        uint16_t track_count;
        uint16_t delta_unit;
    } fmidi_smf_info_t;

    typedef struct fmidi_track_iter
    {
        uint16_t track;
        uint32_t index;
    } fmidi_track_iter_t;

    typedef struct fmidi_seq_event
    {
        double time;
        uint16_t track;
        const fmidi_event_t* event;
    } fmidi_seq_event_t;

    typedef enum fmidi_status
    {
        fmidi_ok,
        fmidi_err_format,
        fmidi_err_eof,
        fmidi_err_input,
        fmidi_err_largefile,
        fmidi_err_output
    } fmidi_status_t;

    typedef struct fmidi_error_info
    {
        fmidi_status_t code;
    } fmidi_error_info_t;

    fmidi_smf_t* fmidi_smf_mem_read(const uint8_t* data, size_t length);
    fmidi_smf_t* fmidi_smf_file_read(const char* filename);
    fmidi_smf_t* fmidi_smf_stream_read(FILE* stream);
    void fmidi_smf_free(fmidi_smf_t* smf);

    const fmidi_smf_info_t* fmidi_smf_get_info(const fmidi_smf_t* smf);
    double fmidi_smf_compute_duration(const fmidi_smf_t* smf);

    void fmidi_smf_track_begin(fmidi_track_iter_t* it, uint16_t track);
    const fmidi_event_t* fmidi_smf_track_next(
        const fmidi_smf_t* smf, fmidi_track_iter_t* it);

    fmidi_seq_t* fmidi_seq_new(const fmidi_smf_t* smf);
    void fmidi_seq_free(fmidi_seq_t* seq);
    void fmidi_seq_rewind(fmidi_seq_t* seq);
    int fmidi_seq_peek_event(fmidi_seq_t* seq, fmidi_seq_event_t* evt);
    int fmidi_seq_next_event(fmidi_seq_t* seq, fmidi_seq_event_t* evt);

    fmidi_player_t* fmidi_player_new(fmidi_smf_t* smf);
    void fmidi_player_tick(fmidi_player_t* player, double delta);
    void fmidi_player_free(fmidi_player_t* player);
    void fmidi_player_start(fmidi_player_t* player);
    void fmidi_player_stop(fmidi_player_t* player);
    void fmidi_player_rewind(fmidi_player_t* player);
    int fmidi_player_running(const fmidi_player_t* player);
    double fmidi_player_current_time(const fmidi_player_t* player);
    void fmidi_player_goto_time(fmidi_player_t* player, double time);
    double fmidi_player_current_speed(const fmidi_player_t* player);
    void fmidi_player_set_speed(fmidi_player_t* player, double speed);
    void fmidi_player_event_callback(
        fmidi_player_t* player,
        void (*callback)(const fmidi_event_t*, void*),
        void* userdata);
    void fmidi_player_finish_callback(
        fmidi_player_t* player,
        void (*callback)(void*),
        void* userdata);

    fmidi_status_t fmidi_errno(void);
    const fmidi_error_info_t* fmidi_errinfo(void);
    const char* fmidi_strerror(fmidi_status_t status);

#ifdef __cplusplus
}
#endif
