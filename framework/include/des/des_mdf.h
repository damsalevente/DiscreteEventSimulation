#ifndef DES_MDF_H
#define DES_MDF_H

#include "des_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal ASAM MDF 4.10 writer.
 *
 * The export produces one measurement channel per resource *instance*.
 * Each channel carries a time-series where every sample is the resource
 * instance's current FSM state (an enum value). A text-list channel
 * conversion (CCBLOCK type 4) maps the integer state values to the
 * human-readable state names declared in the simulation config.
 *
 * Layout on disk:
 *   ID  -> HD -> DG -> [ CG -> (CN time + CN state + CC) -> DT ] x N -> TX*
 */

/* Write resource-utilisation records as an MDF4 file.
 * Returns DES_OK on success, otherwise a DesErrorCode value. */
DesErrorCode DesMdf_exportResources(const struct DesEngine *engine,
                                     const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* DES_MDF_H */
