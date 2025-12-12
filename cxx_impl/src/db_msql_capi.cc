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

#include "db_msql_capi.h"

#include "db_msql.h"

MSQL_Connection MSQL_new(const char *addr, uint16_t port, const char *user,
                         const char *pass, const char *dbname) {
  auto conn_opt =
      PMA_MSQL::Connection::connect_msql(addr, port, user, pass, dbname);
  if (!conn_opt.has_value()) {
    return nullptr;
  }

  void *conn = std::malloc(sizeof(PMA_MSQL::Connection));
  new (conn) PMA_MSQL::Connection(std::move(conn_opt.value()));

  return conn;
}

void MSQL_cleanup(MSQL_Connection *conn) {
  if (!conn || !(*conn)) {
    return;
  }
  PMA_MSQL::Connection *actual =
      reinterpret_cast<PMA_MSQL::Connection *>(*conn);
  actual->~Connection();
  std::free(*conn);
  *conn = nullptr;
}

int MSQL_is_valid(MSQL_Connection conn) {
  if (!conn) {
    return 1;
  }
  PMA_MSQL::Connection *actual = reinterpret_cast<PMA_MSQL::Connection *>(conn);

  return actual->is_valid() ? 0 : 1;
}

int MSQL_ping(MSQL_Connection conn) {
  if (!conn) {
    return 1;
  }
  PMA_MSQL::Connection *actual = reinterpret_cast<PMA_MSQL::Connection *>(conn);
  return actual->ping_check() ? 0 : 1;
}

MSQL_Params MSQL_create_params() {
  void *params = std::malloc(sizeof(std::vector<PMA_MSQL::Value>));
  new (params) std::vector<PMA_MSQL::Value>();

  return params;
}

void MSQL_append_param_null(MSQL_Params params) {
  if (!params) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);
  values->emplace_back();
}

void MSQL_append_param_int64(MSQL_Params params, int64_t value) {
  if (!params) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);
  values->push_back(PMA_MSQL::Value::new_int(value));
}

void MSQL_append_param_uint64(MSQL_Params params, uint64_t value) {
  if (!params) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);
  values->push_back(PMA_MSQL::Value::new_uint(value));
}

void MSQL_append_param_str(MSQL_Params params, const char *value) {
  if (!params) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);
  values->emplace_back(value);
}

void MSQL_append_param_double(MSQL_Params params, double value) {
  if (!params) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);
  values->emplace_back(value);
}

void MSQL_cleanup_params(MSQL_Params *params) {
  if (!params || !(*params)) {
    return;
  }

  std::vector<PMA_MSQL::Value> *values =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(*params);
  values->~vector();
  std::free(*params);
  *params = nullptr;
}

MSQL_Rows MSQL_query(MSQL_Connection conn, const char *stmt,
                     MSQL_Params params) {
  if (!conn) {
    return nullptr;
  }

  PMA_MSQL::Connection *actual = reinterpret_cast<PMA_MSQL::Connection *>(conn);
  std::vector<PMA_MSQL::Value> *v =
      reinterpret_cast<std::vector<PMA_MSQL::Value> *>(params);

  auto stmt_ret_opt = actual->execute_stmt(stmt, *v);
  if (!stmt_ret_opt.has_value()) {
    return nullptr;
  }

  void *rows = std::malloc(sizeof(std::vector<std::vector<PMA_MSQL::Value> >));
  new (rows) std::vector<std::vector<PMA_MSQL::Value> >(
      std::move(stmt_ret_opt.value()));

  return rows;
}

size_t MSQL_row_count(MSQL_Rows rows) {
  if (!rows) {
    return 0;
  }

  std::vector<std::vector<PMA_MSQL::Value> > *v =
      reinterpret_cast<std::vector<std::vector<PMA_MSQL::Value> > *>(rows);

  return v->size();
}

MSQL_Value MSQL_fetch(MSQL_Rows rows, size_t row_idx, size_t col_idx) {
  if (!rows) {
    return nullptr;
  }

  std::vector<std::vector<PMA_MSQL::Value> > *v =
      reinterpret_cast<std::vector<std::vector<PMA_MSQL::Value> > *>(rows);

  if (v->size() <= row_idx) {
    return nullptr;
  } else if (v->at(row_idx).size() <= col_idx) {
    return nullptr;
  }

  void *val = std::malloc(sizeof(PMA_MSQL::Value));
  new (val) PMA_MSQL::Value(v->at(row_idx).at(col_idx));

  return val;
}

void MSQL_cleanup_rows(MSQL_Rows *rows) {
  if (!rows || !(*rows)) {
    return;
  }

  std::vector<std::vector<PMA_MSQL::Value> > *v =
      reinterpret_cast<std::vector<std::vector<PMA_MSQL::Value> > *>(*rows);

  v->~vector();
  std::free(*rows);
  *rows = nullptr;
}

int MSQL_get_type(MSQL_Value value) {
  if (!value) {
    return 0;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(value);

  switch (v->get_type()) {
    case PMA_MSQL::Value::INV_NULL:
      return 1;
    case PMA_MSQL::Value::STRING:
      return 4;
    case PMA_MSQL::Value::SIGNED_INT:
      return 2;
    case PMA_MSQL::Value::UNSIGNED_INT:
      return 3;
    case PMA_MSQL::Value::DOUBLE:
      return 5;
    default:
      return 0;
  }
}

const int64_t *MSQL_get_int64(MSQL_Value value) {
  if (!value) {
    return nullptr;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(value);

  auto v_opt = v->get_signed_int();
  if (v_opt.has_value()) {
    return v_opt.value().get();
  } else {
    return nullptr;
  }
}

const uint64_t *MSQL_get_uint64(MSQL_Value value) {
  if (!value) {
    return nullptr;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(value);

  auto v_opt = v->get_unsigned_int();
  if (v_opt.has_value()) {
    return v_opt.value().get();
  } else {
    return nullptr;
  }
}

const double *MSQL_get_double(MSQL_Value value) {
  if (!value) {
    return nullptr;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(value);

  auto v_opt = v->get_double();
  if (v_opt.has_value()) {
    return v_opt.value().get();
  } else {
    return nullptr;
  }
}

const char *MSQL_get_str(MSQL_Value value) {
  if (!value) {
    return nullptr;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(value);

  auto v_opt = v->get_str();
  if (v_opt.has_value()) {
    return v_opt.value()->c_str();
  } else {
    return nullptr;
  }
}

void MSQL_cleanup_value(MSQL_Value *value) {
  if (!value || !(*value)) {
    return;
  }

  PMA_MSQL::Value *v = reinterpret_cast<PMA_MSQL::Value *>(*value);
  v->~Value();
  std::free(*value);
  *value = nullptr;
}
