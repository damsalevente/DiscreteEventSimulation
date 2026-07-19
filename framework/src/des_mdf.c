#include "des/des_mdf.h"
#include "des/des_engine.h"
#include "des/des_stats.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* ----------------------------------------------------------------------------
 * MDF 4.10 block helpers (layout matches the ASAM MDF 4.10 fixed-size blocks
 * as read by asammdf).
 *
 * Every block begins with:
 *   char   id[4];        // e.g. "##HD", "##DG", ...
 *   uint32 reserved;     // 4-byte reserved field (always 0)
 *   uint64 block_len;    // total block size in bytes
 *   uint64 links_nr;     // number of links that follow
 *   uint64 links[links_nr];
 *   uint8  data[];       // remaining bytes (fixed per block type)
 * All multi-byte integers are little-endian.
 * -------------------------------------------------------------------------- */

typedef struct {
    FILE   *fp;
    uint64_t first_cg;      /* first channel group (written into DG) */
    uint64_t prev_cg;       /* for chaining CG -> next CG */
    int      cg_count;
} MdfWriter;

/* Block ids (4 chars; MDF4 uses a "##" prefix on the id). */
#define MDF_ID_MDF  "##MD"
#define MDF_ID_HD   "##HD"
#define MDF_ID_DG   "##DG"
#define MDF_ID_CG   "##CG"
#define MDF_ID_CN   "##CN"
#define MDF_ID_CC   "##CC"
#define MDF_ID_TX   "##TX"
#define MDF_ID_DT   "##SD"

static uint64_t mdf_tell(MdfWriter *w) {
    return (uint64_t)ftell(w->fp);
}

/* Write a TXBLOCK: a UTF-8 string (no BOM), not null terminated.
 * Layout (matches asammdf): id(4) + reserved(4) + block_len(8)
 *         + links_nr(8) + tx_next(8) + text. (No separate flags byte.) */
static uint64_t mdf_write_tx(MdfWriter *w, const char *text) {
    size_t len = strlen(text);
    char id[4]; memcpy(id, MDF_ID_TX, 4);
    uint64_t size = 4 + 4 + 8 + 8 + 8 + (uint64_t)len;
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;
    uint64_t links = 1;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&links, 8, 1, w->fp);
    fwrite(&zero, 8, 1, w->fp);          /* link 0: tx_next (0 = none) */
    fwrite(text, 1, len, w->fp);
    return pos;
}

/* Write a DT (data) block holding arbitrary bytes. Returns file offset.
 * Layout (matches asammdf FMT_DATA_BLOCK <4sI2Q{}s>): id(4) + reserved(4)
 *         + block_len(8) + links_nr(8) + data. (No link slot.) */
static uint64_t mdf_write_dt(MdfWriter *w, const void *data, uint64_t nbytes) {
    char id[4]; memcpy(id, MDF_ID_DT, 4);
    uint64_t size = 4 + 4 + 8 + 8 + nbytes;
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;
    uint64_t links = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&links, 8, 1, w->fp);
    if (nbytes > 0) fwrite(data, 1, nbytes, w->fp);
    return pos;
}

/* Write a CCBLOCK of type NONE (identity conversion). 80 bytes.
 * Layout: id(4)+reserved(4)+block_len(8)+links_nr(8)+6 links + 2B+3H+2d. */
static uint64_t mdf_write_cc_none(MdfWriter *w) {
    char id[4]; memcpy(id, MDF_ID_CC, 4);
    uint64_t link_count = 6;             /* next + 5 reserved */
    uint64_t data_bytes = 2 + 2 + 2 + 8 + 8;  /* 2B + 3H + 2d */
    uint64_t size = 4 + 4 + 8 + 8 + link_count * 8 + data_bytes;
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&link_count, 8, 1, w->fp);
    for (int i = 0; i < 6; i++)
        fwrite(&zero, 8, 1, w->fp);      /* links */

    uint8_t conv_type = 0;               /* NONE */
    uint8_t precision = 0;
    uint16_t flags = 0;
    uint16_t refs = 0;
    uint16_t val_count = 0;
    double dmin = 0.0, dmax = 0.0;
    fwrite(&conv_type, 1, 1, w->fp);
    fwrite(&precision, 1, 1, w->fp);
    fwrite(&flags, 2, 1, w->fp);
    fwrite(&refs, 2, 1, w->fp);
    fwrite(&val_count, 2, 1, w->fp);
    fwrite(&dmin, 8, 1, w->fp);
    fwrite(&dmax, 8, 1, w->fp);
    return pos;
}

/*
 * Write a CNBLOCK (channel). Fixed 160-byte layout
 * (<4sI10Q4B4I2BH6d>, FMT_SIMPLE_CHANNEL):
 *   links (8): next_ch, component, name, source, conversion,
 *              data_block, unit, comment
 *   data: 4B (cn_type, sync_type, data_type, bit_offset)
 *       + 4I (bit_count, flags, invalidation_bit, +1 reserved)
 *       + 2B + H (reserved) + 6d (ranges/limits)
 */
static uint64_t mdf_write_cn(MdfWriter *w,
                               uint64_t cn_next,
                               uint64_t cn_conversion,
                               uint64_t cn_data,
                               uint64_t cn_unit_tx,
                               uint64_t cn_comment_tx,
                               const char *name,
                               int data_type,
                               int bit_count,
                               int byte_off_param,
                               int cell_size,
                               int samples) {
    char id[4]; memcpy(id, MDF_ID_CN, 4);

    /* Write the name TX block FIRST so its offset is known before the
     * CN link section is written (mdf_write_tx advances the file pointer). */
    uint64_t name_tx = mdf_write_tx(w, name);

    uint64_t link_count = 8;
    uint64_t data_bytes = 4 + 4 * 4 + 2 + 2 + 6 * 8;  /* 4B + 4I + 2B + H + 6d */
    uint64_t size = 4 + 4 + 8 + 8 + link_count * 8 + data_bytes;

    /* CN block starts here (after the name TX block). */
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&link_count, 8, 1, w->fp);

    fwrite(&cn_next, 8, 1, w->fp);           /* link 0: next_ch */
    fwrite(&zero, 8, 1, w->fp);              /* link 1: component (0) */
    fwrite(&name_tx, 8, 1, w->fp);           /* link 2: name */
    fwrite(&zero, 8, 1, w->fp);              /* link 3: source (0) */
    fwrite(&cn_conversion, 8, 1, w->fp);     /* link 4: conversion */
    fwrite(&cn_data, 8, 1, w->fp);           /* link 5: data_block */
    fwrite(&cn_unit_tx, 8, 1, w->fp);        /* link 6: unit */
    fwrite(&cn_comment_tx, 8, 1, w->fp);     /* link 7: comment */

    uint8_t cn_type = 2;                 /* 2 = channel with fixed data */
    uint8_t sync_type = 0;
    /* asammdf encodes int32 channels with data_type code 4 (matches the
     * reference file it writes). */
    uint8_t dt = (uint8_t)data_type;     /* 4 = signed int32 (asammdf code) */
    uint8_t bit_off = 0;
    uint32_t byte_offset = (uint32_t)byte_off_param;
    uint32_t bit_cnt = (uint32_t)bit_count;  /* bits (32 for int32) */
    uint32_t flags = 0;
    uint32_t inv_bit = 0;
    uint16_t reserved1 = 3;              /* matches reference (reserved1=3) */
    uint16_t attachment_nr = 0;
    uint16_t precision = 0;
    double dmin = 0.0, dmax = 0.0, lmin = 0.0, lmax = 0.0, lemin = 0.0, lemax = 0.0;

    fwrite(&cn_type, 1, 1, w->fp);
    fwrite(&sync_type, 1, 1, w->fp);
    fwrite(&dt, 1, 1, w->fp);
    fwrite(&bit_off, 1, 1, w->fp);
    fwrite(&byte_offset, 4, 1, w->fp);
    fwrite(&bit_cnt, 4, 1, w->fp);
    fwrite(&flags, 4, 1, w->fp);
    fwrite(&inv_bit, 4, 1, w->fp);
    fwrite(&reserved1, 2, 1, w->fp);
    fwrite(&attachment_nr, 2, 1, w->fp);
    fwrite(&precision, 2, 1, w->fp);
    fwrite(&dmin, 8, 1, w->fp);
    fwrite(&dmax, 8, 1, w->fp);
    fwrite(&lmin, 8, 1, w->fp);
    fwrite(&lmax, 8, 1, w->fp);
    fwrite(&lemin, 8, 1, w->fp);
    fwrite(&lemax, 8, 1, w->fp);
    (void)cell_size; (void)samples;
    return pos;
}

/* Write a CGBLOCK (channel group). Fixed 104-byte layout
 * (<4sI10Q2H3I>):
 *   links: next_cg, first_ch, acq_name, acq_source, sample_red, comment,
 *          +2 reserved (8 links)
 *   data: 2H (record_id, cycles_nr) + 3I (samples_byte_nr, invalidation, ...) */
static uint64_t mdf_write_cg(MdfWriter *w,
                               uint64_t cg_next,
                               uint64_t cg_first_cn,
                               int record_id,
                               int num_samples,
                               int record_size) {
    char id[4]; memcpy(id, MDF_ID_CG, 4);
    uint64_t link_count = 8;
    uint64_t data_bytes = 2 + 2 + 4 + 4 + 4;  /* 2H + 3I */
    uint64_t size = 4 + 4 + 8 + 8 + link_count * 8 + data_bytes;
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&link_count, 8, 1, w->fp);

    fwrite(&cg_next, 8, 1, w->fp);       /* link 0: next_cg */
    fwrite(&cg_first_cn, 8, 1, w->fp);   /* link 1: first_ch */
    fwrite(&zero, 8, 1, w->fp);          /* link 2: acq_name */
    fwrite(&zero, 8, 1, w->fp);          /* link 3: acq_source */
    fwrite(&zero, 8, 1, w->fp);          /* link 4: sample_reduction */
    fwrite(&zero, 8, 1, w->fp);          /* link 5: comment */
    fwrite(&zero, 8, 1, w->fp);          /* link 6: reserved */
    fwrite(&zero, 8, 1, w->fp);          /* link 7: reserved */

    uint16_t rid = (uint16_t)record_id;
    uint16_t cycles = (uint16_t)num_samples;
    uint32_t samples_byte_nr = (uint32_t)(record_size * num_samples);
    uint32_t invalidation = 0;
    uint32_t rec_size = (uint32_t)record_size;
    fwrite(&rid, 2, 1, w->fp);
    fwrite(&cycles, 2, 1, w->fp);
    fwrite(&samples_byte_nr, 4, 1, w->fp);
    fwrite(&invalidation, 4, 1, w->fp);
    fwrite(&rec_size, 4, 1, w->fp);
    return pos;
}

/* Write a DGBLOCK (data group). Fixed 64-byte layout
 * (<4sI6QB7s>):
 *   links: next_dg, first_cg, data_block, comment (4 links)
 *   data: 1 byte + 7 padding */
static uint64_t mdf_write_dg(MdfWriter *w,
                               uint64_t dg_next,
                               uint64_t dg_cg_first,
                               uint64_t dg_data) {
    char id[4]; memcpy(id, MDF_ID_DG, 4);
    uint64_t link_count = 4;
    uint64_t data_bytes = 1 + 7;         /* B + 7s */
    uint64_t size = 4 + 4 + 8 + 8 + link_count * 8 + data_bytes;
    uint64_t pos = mdf_tell(w);
    uint64_t zero = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&link_count, 8, 1, w->fp);

    fwrite(&dg_next, 8, 1, w->fp);       /* link 0: next_dg */
    fwrite(&dg_cg_first, 8, 1, w->fp);   /* link 1: first_cg */
    fwrite(&dg_data, 8, 1, w->fp);       /* link 2: data_block */
    fwrite(&zero, 8, 1, w->fp);          /* link 3: comment */

    uint8_t rec_id = 1;
    fwrite(&rec_id, 1, 1, w->fp);
    uint8_t pad[7] = {0};
    fwrite(pad, 1, 7, w->fp);
    return pos;
}

/* Write IDBLOCK + HDBLOCK.
 * HD layout (<4sI9Q2h4B2Q>, 104 bytes):
 *   links: first_dg, file_history, channel_tree, first_attachment,
 *          first_event, comment, +2 reserved (8 links)
 *   data: 2h (tz_offset, dst) + 4B (time_flags, time_quality, +2 reserved)
 *       + 2Q (abs_time, +1 reserved) */
static void mdf_write_header(MdfWriter *w, const char *comment) {
    /* ---- IDBLOCK (64 bytes) ---- */
    char idblock[64];
    memset(idblock, 0, sizeof(idblock));
    memcpy(idblock + 0,  "MDF     ", 8);          /* file identifier */
    memcpy(idblock + 8,  "4.10    ", 8);          /* version string */
    memcpy(idblock + 16, "MDF     ", 8);          /* program id (placeholder) */
    memcpy(idblock + 24, "Unified ", 8);
    memcpy(idblock + 32, "Desktop  ", 8);
    memcpy(idblock + 40, "Sim     ", 8);
    memcpy(idblock + 48, "MDF4 Writ", 8);
    memcpy(idblock + 56, "er      ", 8);
    fwrite(idblock, 1, 64, w->fp);

    /* ---- HDBLOCK (must immediately follow the IDBLOCK at offset 64) ---- */
    char id[4]; memcpy(id, MDF_ID_HD, 4);
    uint64_t link_count = 8;
    uint64_t data_bytes = 2 + 2 + 4 + 2 + 2 + 8 + 8;  /* 2h + 4B + 2Q */
    uint64_t size = 4 + 4 + 8 + 8 + link_count * 8 + data_bytes;

    uint64_t pos = mdf_tell(w);          /* should be 64 */
    uint64_t zero = 0;

    fwrite(id, 1, 4, w->fp);
    fwrite(&zero, 4, 1, w->fp);          /* reserved */
    fwrite(&size, 8, 1, w->fp);
    fwrite(&link_count, 8, 1, w->fp);

    /* links: first_dg(0, patched), file_history, channel_tree,
     *        first_attachment, first_event, comment, reserved, reserved */
    fwrite(&zero, 8, 1, w->fp);          /* link 0: first_dg (patched later) */
    fwrite(&zero, 8, 1, w->fp);          /* link 1: file_history */
    fwrite(&zero, 8, 1, w->fp);          /* link 2: channel_tree */
    fwrite(&zero, 8, 1, w->fp);          /* link 3: first_attachment */
    fwrite(&zero, 8, 1, w->fp);          /* link 4: first_event */
    fwrite(&zero, 8, 1, w->fp);          /* link 5: comment (patched below) */
    fwrite(&zero, 8, 1, w->fp);          /* link 6: reserved */
    fwrite(&zero, 8, 1, w->fp);          /* link 7: reserved */

    /* HD data fields */
    uint16_t tz_offset = 0;
    uint16_t dst = 0;
    uint8_t time_flags = 2;              /* local PC time */
    uint8_t time_quality = 0;
    uint8_t reserved_b2 = 0;
    uint8_t reserved_b3 = 0;
    uint64_t abs_time = 0;
    uint64_t reserved_q = 0;

    fwrite(&tz_offset, 2, 1, w->fp);
    fwrite(&dst, 2, 1, w->fp);
    fwrite(&time_flags, 1, 1, w->fp);
    fwrite(&time_quality, 1, 1, w->fp);
    fwrite(&reserved_b2, 1, 1, w->fp);
    fwrite(&reserved_b3, 1, 1, w->fp);
    fwrite(&abs_time, 8, 1, w->fp);
    fwrite(&reserved_q, 8, 1, w->fp);

    /* Write the comment TX block and patch HD.comment (link 5). */
    uint64_t comment_tx = mdf_write_tx(w, comment);
    uint64_t tx_link_off = pos + 4 + 4 + 8 + 8 + 5 * 8;  /* link index 5 */
    fseek(w->fp, (long)tx_link_off, SEEK_SET);
    fwrite(&comment_tx, 8, 1, w->fp);
    fseek(w->fp, 0, SEEK_END);

    w->first_cg = 0;
    w->prev_cg = 0;
    w->cg_count = 0;
}

/* Patch HD.first_dg link (link index 0, at HD + 24). */
static void mdf_patch_hd_dg(MdfWriter *w, uint64_t dg_off) {
    uint64_t off = 64 + 4 + 4 + 8 + 8;   /* id + reserved + block_len + links_nr */
    fseek(w->fp, (long)off, SEEK_SET);
    fwrite(&dg_off, 8, 1, w->fp);
    fseek(w->fp, 0, SEEK_END);
}

/* ----------------------------------------------------------------------------
 * Sample data assembly
 * -------------------------------------------------------------------------- */

/* One resource instance's sampled timeline. */
typedef struct {
    char     name[128];
    int      num_states;
    char     state_names[16][DES_MAX_NAME];
    int     *times;       /* ascending */
    int     *states;      /* enum values */
    int      count;
    int      cap;
} InstanceSeries;

static void series_push(InstanceSeries *s, int t, int state) {
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->times  = (int *)realloc(s->times, (size_t)s->cap * sizeof(int));
        s->states = (int *)realloc(s->states, (size_t)s->cap * sizeof(int));
    }
    s->times[s->count]  = t;
    s->states[s->count] = state;
    s->count++;
}

DesErrorCode DesMdf_exportResources(const struct DesEngine *engine,
                                     const char *filepath) {
    const DesStatsCollector *stats = &engine->stats;

    if (stats->num_resource_records == 0) {
        fprintf(stderr,
                "[MDF] No resource records. Enable 'record_resource_util' in config.\n");
        return DES_ERR_NO_RESOURCE;
    }

    /* Total resource instances. */
    int total_inst = engine->total_resource_instances;

    InstanceSeries *series = (InstanceSeries *)calloc((size_t)total_inst, sizeof(InstanceSeries));
    if (!series) return DES_ERR_OUT_OF_MEMORY;

    for (int i = 0; i < total_inst; i++) {
        series[i].cap = 0;
        series[i].count = 0;
        series[i].times = NULL;
        series[i].states = NULL;
    }

    /* Resource records represent occupancy, independently of stage FSM state. */
    for (int inst = 0; inst < total_inst; inst++) {
        int type_id = engine->resource_instances[inst].type_id;
        const char *type_name = engine->resource_types[type_id].name;
        snprintf(series[inst].name, sizeof(series[inst].name),
                 "%s[%d]", type_name, engine->resource_instances[inst].instance_id);
        series[inst].num_states = 2;
        strncpy(series[inst].state_names[0], "IDLE", DES_MAX_NAME - 1);
        strncpy(series[inst].state_names[1], "BUSY", DES_MAX_NAME - 1);
    }

    /* Populate per-instance series from records. */
    for (int r = 0; r < stats->num_resource_records; r++) {
        const DesResourceRecord *rec = &stats->resource_records[r];
        /* instance index = type.first_instance_idx + instance_id */
        int type_id = rec->resource_type_id;
        if (type_id < 0 || type_id >= engine->num_resource_types) continue;
        int inst_idx = engine->resource_types[type_id].first_instance_idx + rec->instance_id;
        if (inst_idx < 0 || inst_idx >= total_inst) continue;
        series_push(&series[inst_idx], rec->time, rec->state);
    }

    /* Open output file. */
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        free(series);
        return DES_ERR_FILE_IO;
    }

    MdfWriter w;
    memset(&w, 0, sizeof(w));
    w.fp = fp;

    mdf_write_header(&w, "Resource state enumeration export (MDF4)");

    /* Write one CG per instance. Each channel (time, state) points to its
     * own DT block via the CN data_block link; the CG data_block link is 0. */
    for (int inst = 0; inst < total_inst; inst++) {
        InstanceSeries *s = &series[inst];
        if (s->count == 0) continue;

        /* Per-channel data blocks (int32). */
        uint64_t dt_time  = mdf_write_dt(&w, s->times,  (uint64_t)s->count * sizeof(int));
        uint64_t dt_state = mdf_write_dt(&w, s->states, (uint64_t)s->count * sizeof(int));

        /* Channel conversion: identity (samples stored as raw enum ints). */
        uint64_t cc = mdf_write_cc_none(&w);

        /* State channel (CN) with conversion. */
        char state_name[160];
        snprintf(state_name, sizeof(state_name), "%s.State", s->name);
        char unit[64];
        snprintf(unit, sizeof(unit), "enum:%s", s->name);
        uint64_t cn_unit_tx = mdf_write_tx(&w, unit);
        uint64_t cn_state_comment = mdf_write_tx(&w, s->state_names[0]);
        uint64_t cn_state = mdf_write_cn(&w, 0, cc, dt_state, cn_unit_tx,
                                         cn_state_comment, state_name,
                                         1 /*signed int*/, 32, 0, 4, s->count);

        /* Time channel (CN), data_type 1 (signed int) as integer ticks. */
        char time_name[160];
        snprintf(time_name, sizeof(time_name), "%s.Time", s->name);
        uint64_t cn_time_unit = mdf_write_tx(&w, "tick");
        uint64_t cn_time_comment = mdf_write_tx(&w, "tick");
        uint64_t cn_time = mdf_write_cn(&w, cn_state, 0, dt_time, cn_time_unit,
                                         cn_time_comment, time_name,
                                         1 /*signed int*/, 32, 0, 4, s->count);

        /* Channel group. */
        int record_size = 8; /* two int32 */
        uint64_t cg = mdf_write_cg(&w, 0, cn_time,
                                    w.cg_count + 1, s->count, record_size);

        if (w.cg_count == 0) {
            w.first_cg = cg;
        } else {
            /* patch previous CG next link (link 0 at CG + 24) */
            uint64_t off = w.prev_cg + 4 + 4 + 8 + 8;  /* id+reserved+block_len+links_nr */
            fseek(fp, (long)off, SEEK_SET);
            fwrite(&cg, 8, 1, fp);
            fseek(fp, 0, SEEK_END);
        }
        w.prev_cg = cg;
        w.cg_count++;
    }

    /* Write single DG pointing to first CG. */
    uint64_t dg = mdf_write_dg(&w, 0, w.first_cg, 0);
    mdf_patch_hd_dg(&w, dg);

    /* Cleanup. */
    for (int i = 0; i < total_inst; i++) {
        free(series[i].times);
        free(series[i].states);
    }
    free(series);
    fclose(fp);

    printf("[MDF] Wrote %d resource channels to %s\n", w.cg_count, filepath);
    return DES_OK;
}
