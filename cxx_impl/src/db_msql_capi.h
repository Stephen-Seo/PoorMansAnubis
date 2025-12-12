// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifdef __cplusplus
#include <cstdint>
#include <cstdlib>

extern "C" {
#else
#include <stdint.h>
#include <stdlib.h>
#endif

typedef void *MSQL_Connection;
typedef void *MSQL_Rows;
typedef void *MSQL_Params;
typedef void *MSQL_Value;

/// Returns NULL on failure.
extern MSQL_Connection MSQL_new(const char *addr, uint16_t port,
                                const char *user, const char *pass,
                                const char *dbname);
extern void MSQL_cleanup(MSQL_Connection *conn);

/// Returns 0 on success.
extern int MSQL_is_valid(MSQL_Connection conn);
/// Returns 0 on success.
extern int MSQL_ping(MSQL_Connection conn);

extern MSQL_Params MSQL_create_params();
extern void MSQL_append_param_null(MSQL_Params params);
extern void MSQL_append_param_int64(MSQL_Params params, int64_t value);
extern void MSQL_append_param_uint64(MSQL_Params params, uint64_t value);
extern void MSQL_append_param_str(MSQL_Params params, const char *value);
extern void MSQL_append_param_double(MSQL_Params params, double value);
extern void MSQL_cleanup_params(MSQL_Params *params);

/// Returns NULL on failure.
extern MSQL_Rows MSQL_query(MSQL_Connection conn, const char *stmt,
                            MSQL_Params params);

extern size_t MSQL_row_count(MSQL_Rows rows);

/// Returns NULL on error. Returned MSQL_Value must be cleaned up with
/// MSQL_cleanup_value().
extern MSQL_Value MSQL_fetch(MSQL_Rows rows, size_t row_idx, size_t col_idx);

extern void MSQL_cleanup_rows(MSQL_Rows *rows);

/// 0 for error, 1 for null, 2 for int64, 3 for uint64, 4 for string, 5 for
/// double.
extern int MSQL_get_type(MSQL_Value value);

// Non-null if valid type.
extern const int64_t *MSQL_get_int64(MSQL_Value value);
// Non-null if valid type.
extern const uint64_t *MSQL_get_uint64(MSQL_Value value);
// Non-null if valid type.
extern const double *MSQL_get_double(MSQL_Value value);
// Non-null if valid type.
extern const char *MSQL_get_str(MSQL_Value value);
extern void MSQL_cleanup_value(MSQL_Value *value);

#ifdef __cplusplus
}
#endif
