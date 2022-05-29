// Copyright (c 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"

enum Order {
  SEQUENTIAL,
  RANDOM
};

enum DBState {
  FRESH,
  EXISTING
};

sqlite3* db_;
int db_num_;
int num_;
int reads_;
double start_;
double last_op_finish_;
int64_t bytes_;
char* message_ = NULL;
Histogram hist_;
Raw raw_;
RandomGenerator gen_;
Random rand_;

/* State kept for progress messages */
int done_;
int next_report_;

static void print_header(void);
static void print_warnings(void);
static void print_environment(void);
static void start(void);
static void stop(const char *name);

inline
static void exec_error_check(int status, char *err_msg) {
  if (status != SQLITE_OK) {
    SQLITE_BENCHMARK_LOG(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    #ifndef SQLITE_OS_QUEC_RTOS
    exit(1);
    #endif
  }
}

inline
static void step_error_check(int status) {
  if (status != SQLITE_DONE) {
    SQLITE_BENCHMARK_LOG(stderr, "SQL step error: status = %d\n", status);
    #ifndef SQLITE_OS_QUEC_RTOS
    exit(1);
    #endif
  }
}

inline
static void error_check(int status) {
  if (status != SQLITE_OK) {
    SQLITE_BENCHMARK_LOG(stderr, "sqlite3 error: status = %d\n", status);
    #ifndef SQLITE_OS_QUEC_RTOS
    exit(1);
    #endif
  }
}

inline
static void wal_checkpoint(sqlite3* db_) {
  /* Flush all writes to disk */
  if (FLAGS_WAL_enabled) {
    sqlite3_wal_checkpoint_v2(db_, NULL, SQLITE_CHECKPOINT_FULL, NULL,
                              NULL);
  }
}

static void print_header() {
  const int kKeySize = 16;
  print_environment();
  SQLITE_BENCHMARK_LOG(stderr, "Keys:       %d bytes each\n", kKeySize);
  SQLITE_BENCHMARK_LOG(stderr, "Values:     %d bytes each\n", FLAGS_value_size);  
  SQLITE_BENCHMARK_LOG(stderr, "Entries:    %d\n", num_);
  if(((int64_t)(kKeySize + FLAGS_value_size) * num_) > 1048576.0)
  {
    SQLITE_BENCHMARK_LOG(stderr, "RawSize:    %.1f MB (estimated)\n",
            (((int64_t)(kKeySize + FLAGS_value_size) * num_)
            / 1048576.0));
  }
  else
  {
    SQLITE_BENCHMARK_LOG(stderr, "RawSize:    %.1f KB (estimated)\n",
            (((int64_t)(kKeySize + FLAGS_value_size) * num_)
            / 1024.0));
  }
  
  print_warnings();
  SQLITE_BENCHMARK_LOG(stderr, "------------------------------------------------\n");
}

static void print_warnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
  SQLITE_BENCHMARK_LOG(stderr,
      "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
      );
#endif
#ifndef NDEBUG
  SQLITE_BENCHMARK_LOG(stderr,
      "WARNING: Assertions are enabled: benchmarks unnecessarily slow\n"
      );
#endif
}

static void print_environment() {
  SQLITE_BENCHMARK_LOG(stderr, "SQLite:     version %s\n", SQLITE_VERSION);
#if defined(__linux)
  time_t now = time(NULL);
  SQLITE_BENCHMARK_LOG(stderr, "Date:       %s", ctime(&now));

  FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo != NULL) {
    char line[1000];
    int num_cpus = 0;
    char* cpu_type = pvPortMalloc(sizeof(char) * 1000);
    char* cache_size = pvPortMalloc(sizeof(char) * 1000);
    while (fgets(line, sizeof(line), cpuinfo) != NULL) {
      char* sep = strchr(line, ':');
      if (sep == NULL) {
        continue;
      }
      char* key = pvPortCalloc(sizeof(char), 1000);
      char* val = pvPortCalloc(sizeof(char), 1000);
      strncpy(key, line, sep - 1 - line);
      strcpy(val, sep + 1);
      char* trimed_key = trim_space(key);
      char* trimed_val = trim_space(val);
      vPortFree(key);
      vPortFree(val);
      if (!strcmp(trimed_key, "model name")) {
        ++num_cpus;
        strcpy(cpu_type, trimed_val);
      } else if (!strcmp(trimed_key, "cache size")) {
        strcpy(cache_size, trimed_val);
      }
      vPortFree(trimed_key);
      vPortFree(trimed_val);
    }
    fclose(cpuinfo);
    SQLITE_BENCHMARK_LOG(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type);
    SQLITE_BENCHMARK_LOG(stderr, "CPUCache:   %s\n", cache_size);
    vPortFree(cpu_type);
    vPortFree(cache_size);
  }
#endif
}

static void start() {
  start_ =  now_micros() * 1e-6;
  bytes_ = 0;
  if(message_)
  {
    
    vPortFree(message_);
  }
  message_ = pvPortMalloc(sizeof(char) * 10000);
  
  strcpy(message_, "");
  last_op_finish_ = start_;
  histogram_clear(&hist_);
  if (FLAGS_raw)
  {
    raw_clear(&raw_);
  }
  
  done_ = 0;
  next_report_ = 100;
}

void finished_single_op() {
  if (FLAGS_histogram || FLAGS_raw) {
    double now = now_micros() * 1e-6;
    double micros = (now - last_op_finish_) * 1e6;
    if (FLAGS_histogram) {
      histogram_add(&hist_, micros);
      if (micros > 20000) {
        SQLITE_BENCHMARK_LOG(stderr, "long op: %.1f micros%30s\r", micros, "");
        #ifndef SQLITE_OS_QUEC_RTOS
        fflush(stderr);
        #endif
      }
    }
    if (FLAGS_raw) {
      raw_add(&raw_, micros);
    }
    last_op_finish_ = now;
  }

  done_++;
  if (done_ >= next_report_) {
    if      (next_report_ < 1000)   next_report_ += 100;
    else if (next_report_ < 5000)   next_report_ += 500;
    else if (next_report_ < 10000)  next_report_ += 1000;
    else if (next_report_ < 50000)  next_report_ += 5000;
    else if (next_report_ < 100000) next_report_ += 10000;
    else if (next_report_ < 500000) next_report_ += 50000;
    else                            next_report_ += 100000;
    SQLITE_BENCHMARK_LOG(stderr, "... finished %d ops%30s\r", done_, "");
#ifndef SQLITE_OS_QUEC_RTOS
    fflush(stderr);
#endif
  }
#ifdef SQLITE_OS_QUEC_RTOS
  ql_dev_feed_wdt();
#endif
}

static void stop(const char* name) {
  double finish = now_micros() * 1e-6;

  if (done_ < 1) done_ = 1;

  if (bytes_ > 0) {
    char *rate = pvPortMalloc(sizeof(char) * 100);
    memset(rate, 0, sizeof(char) * 100);
    if(bytes_ >= 1048576)
    {
        snprintf(rate, sizeof(char) * 100-1, "%6.1f MB/s",
                  (bytes_ / 1048576.0) / (finish - start_));
    }
    else
    {
        snprintf(rate, sizeof(char) * 100-1, "%6.1f KB/s",
                  (bytes_ / 1024.0) / (finish - start_));
    }

    if (message_ && !strcmp(message_, "")) {
        strcat(rate, " ");
        strcat(rate, message_);
        
        vPortFree(message_);
        message_ = rate;
        //SQLITE_BENCHMARK_LOG(stderr, "%d-%s change:%p\n", __LINE__, __func__, message_);
    } else {
      if(message_)
      {
        
        vPortFree(message_);
      }
      message_ = rate;
      //SQLITE_BENCHMARK_LOG(stderr, "%d-%s change:%p\n", __LINE__, __func__, message_);
    }
  }

  SQLITE_BENCHMARK_LOG(stderr, "%-12s : %11.3f micros/op;%s%s\n",
          name,
          (finish - start_) * 1e6 / done_,
          (!message_ || !strcmp(message_, "") ? "" : " "),
          (!message_) ? "" : message_);
  if(message_)
  {
    
    vPortFree(message_);
    message_ = NULL;
  }
  if (FLAGS_raw) {
    raw_print(stdout, &raw_);
    raw_free(&raw_);
  }
  if (FLAGS_histogram) {
    char *data = histogram_to_string(&hist_);
    SQLITE_BENCHMARK_LOG(stderr, "Microseconds per op:\n%s\n",
            data);
    vPortFree(data);
  }

  if(name)
  {
    vPortFree((void *)name);
  }
#ifndef SQLITE_OS_QUEC_RTOS
  fflush(stdout);
  fflush(stderr);
#endif
}

void benchmark_init() {
  db_ = NULL;
  db_num_ = 0;
  num_ = FLAGS_num;
  reads_ = FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads;
  bytes_ = 0;
  rand_gen_init(&gen_, FLAGS_compression_ratio);
  rand_init(&rand_, 301);;
#ifndef SQLITE_OS_QUEC_RTOS

  if (!FLAGS_use_existing_db) {
      struct dirent* ep;
      DIR* test_dir = opendir(FLAGS_db);
      while ((ep = readdir(test_dir)) != NULL) {
          if (starts_with(ep->d_name, "dbbench_sqlite3")) {
              char file_name[1000];
              strcpy(file_name, FLAGS_db);
              strcat(file_name, ep->d_name);
              remove(file_name);
          }
      }
      closedir(test_dir);
      rmdir(FLAGS_db);
  }
  mkdir(FLAGS_db);
#else
  if (!FLAGS_use_existing_db) {

    ql_rmdir_ex(FLAGS_db);
  }

  ql_mkdir(FLAGS_db, 0);

#endif
}

void benchmark_fini() {
  int status = sqlite3_close(db_);
  error_check(status);
  if(message_)
  {
      
      vPortFree(message_);
  }
  if (FLAGS_raw)
  {
    raw_free(&raw_);
  }
  
  rand_gen_deinit(&gen_);

}

void benchmark_run() {
  print_header();
  benchmark_open();

  char* benchmarks = FLAGS_benchmarks;
  while (benchmarks != NULL) {
    char* sep = strchr(benchmarks, ',');
    char* name;
    if (sep == NULL) {
      name = benchmarks;
      benchmarks = NULL;
    } else {
      name = pvPortCalloc(sizeof(char), (sep - benchmarks + 1));
      strncpy(name, benchmarks, sep - benchmarks);
      benchmarks = sep + 1;
    }
    bytes_ = 0;
    start();
    bool known = true;
    bool write_sync = false;
    if (!strcmp(name, "fillseq")) {
      benchmark_write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseqbatch")) {
      benchmark_write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandom")) {
      benchmark_write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandbatch")) {
      benchmark_write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "overwrite")) {
      benchmark_write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "overwritebatch")) {
      benchmark_write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandsync")) {
      write_sync = true;
      benchmark_write(write_sync, RANDOM, FRESH, num_ / 100, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseqsync")) {
      write_sync = true;
      benchmark_write(write_sync, SEQUENTIAL, FRESH, num_ / 100, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrand100K")) {
      benchmark_write(write_sync, RANDOM, FRESH, num_ / 1000, 100 * 1000, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseq100K")) {
      benchmark_write(write_sync, SEQUENTIAL, FRESH, num_ / 1000, 100 * 1000, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "readseq")) {
      benchmark_read(SEQUENTIAL, 1);
    } else if (!strcmp(name, "readrandom")) {
      benchmark_read(RANDOM, 1);
    } else if (!strcmp(name, "readrand100K")) {
      int n = reads_;
      reads_ /= 1000;
      benchmark_read(RANDOM, 1);
      reads_ = n;
    } else {
      known = false;
      if (strcmp(name, "")) {
        SQLITE_BENCHMARK_LOG(stderr, "unknown benchmark '%s'\n", name);
      }
    }
    if (known) {
      stop(name);
    }
  }
}

void benchmark_open() {
  assert(db_ == NULL);

  int status;
  char file_name[100];
  char* err_msg = NULL;
  db_num_++;

  /* Open database */
  char *tmp_dir = FLAGS_db;
  snprintf(file_name, sizeof(file_name),
            "%sdbbench_sqlite3-%d.db",
            tmp_dir,
            db_num_);
  status = sqlite3_open(file_name, &db_);
  if (status) {
    SQLITE_BENCHMARK_LOG(stderr, "open error: %s\n", sqlite3_errmsg(db_));
    #ifndef SQLITE_OS_QUEC_RTOS
    exit(1);
    #endif
  }
#ifdef SQLITE_OS_QUEC_RTOS
  ql_dev_feed_wdt();
#endif

  /* Change SQLite cache size */
  char cache_size[100];
  snprintf(cache_size, sizeof(cache_size), "PRAGMA cache_size = %d",
            FLAGS_num_pages);
  status = sqlite3_exec(db_, cache_size, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);

  /* FLAGS_page_size is defaulted to 1024 */
  if (FLAGS_page_size != 1024) {
    char page_size[100];
    snprintf(page_size, sizeof(page_size), "PRAGMA page_size = %d",
              FLAGS_page_size);
    status = sqlite3_exec(db_, page_size, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
  }

  /* Change journal mode to WAL if WAL enabled flag is on */
  if (FLAGS_WAL_enabled) {
    char* WAL_stmt = "PRAGMA journal_mode = WAL";

    /* Default cache size is a combined 4 MB */
    char* WAL_checkpoint = "PRAGMA wal_autocheckpoint = 4096";
    status = sqlite3_exec(db_, WAL_stmt, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
    status = sqlite3_exec(db_, WAL_checkpoint, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
  }

  /* Change locking mode to exclusive and create tables/index for database */
  char* locking_stmt = "PRAGMA locking_mode = EXCLUSIVE";
  char* create_stmt =
          "CREATE TABLE test (key blob, value blob, PRIMARY KEY (key))";
  char* stmt_array[] = { locking_stmt, create_stmt, NULL };
  int stmt_array_length = sizeof(stmt_array) / sizeof(char*);
  for (int i = 0; i < stmt_array_length; i++) {
    status = sqlite3_exec(db_, stmt_array[i], NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
#ifdef SQLITE_OS_QUEC_RTOS
    ql_dev_feed_wdt();
#endif
  }
}

void benchmark_write(bool write_sync, int order, int state,
                  int num_entries, int value_size, int entries_per_batch) {
  /* Create new database if state == FRESH */
  if (state == FRESH) {
    if (FLAGS_use_existing_db) {
      if(message_)
      {
        
        vPortFree(message_);
      }
      message_ = pvPortMalloc(sizeof(char) * 100);
      
      strcpy(message_, "skipping (--use_existing_db is true)");
      return;
    }
    sqlite3_close(db_);
    db_ = NULL;
    benchmark_open();
    start();
  }

  if (num_entries != num_) {
    char* msg = pvPortMalloc(sizeof(char) * 100);
    snprintf(msg, 100, "(%d ops)", num_entries);
    if(message_)
    {
      
      vPortFree(message_);
    }
    message_ = msg;
    //SQLITE_BENCHMARK_LOG(stderr, "%d-%s change:%p\n", __LINE__, __func__, message_);
  }

  char* err_msg = NULL;
  int status;

  sqlite3_stmt *replace_stmt, *begin_trans_stmt, *end_trans_stmt;
  char* replace_str = "REPLACE INTO test (key, value) VALUES (?, ?)";
  char* begin_trans_str = "BEGIN TRANSACTION";
  char* end_trans_str = "END TRANSACTION";

  /* Check for synchronous flag in options */
  char* sync_stmt = (write_sync) ? "PRAGMA synchronous = FULL" :
                                    "PRAGMA synchronous = OFF";
  status = sqlite3_exec(db_, sync_stmt, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);

  /* Preparing sqlite3 statements */
  status = sqlite3_prepare_v2(db_, replace_str, -1,
                              &replace_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, begin_trans_str, -1,
                              &begin_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, end_trans_str, -1,
                              &end_trans_stmt, NULL);
  error_check(status);

  bool transaction = (entries_per_batch > 1);
  for (int i = 0; i < num_entries; i += entries_per_batch) {
    /* Begin write transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(begin_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(begin_trans_stmt);
      error_check(status);
    }
#ifdef SQLITE_OS_QUEC_RTOS
    ql_dev_feed_wdt();
#endif

    /* Create and execute SQL statements */
    for (int j = 0; j < entries_per_batch; j++) {

#ifdef SQLITE_OS_QUEC_RTOS
      ql_dev_feed_wdt();
#endif
      const char* value = rand_gen_generate(&gen_, value_size);

      /* Create values for key-value pair */
      const int k = (order == SEQUENTIAL) ? i + j :
                    (rand_next(&rand_) % num_entries);
      char key[100];
      snprintf(key, sizeof(key), "%016d", k);

      /* Bind KV values into replace_stmt */
      status = sqlite3_bind_blob(replace_stmt, 1, key, 16, SQLITE_TRANSIENT);
      error_check(status);
      status = sqlite3_bind_blob(replace_stmt, 2, value,
                                  value_size, SQLITE_TRANSIENT);
      vPortFree((void *)value);
      error_check(status);

      /* Execute replace_stmt */
      bytes_ += value_size + strlen(key);
      status = sqlite3_step(replace_stmt);
      step_error_check(status);

      /* Reset SQLite statement for another use */
      status = sqlite3_clear_bindings(replace_stmt);
      error_check(status);
      status = sqlite3_reset(replace_stmt);
      error_check(status);

      finished_single_op();
    }

    /* End write transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(end_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(end_trans_stmt);
      error_check(status);
    }
  }

  status = sqlite3_finalize(replace_stmt);
  error_check(status);
  status = sqlite3_finalize(begin_trans_stmt);
  error_check(status);
  status = sqlite3_finalize(end_trans_stmt);
  error_check(status);
}

void benchmark_read(int order, int entries_per_batch) {
  int status;
  sqlite3_stmt *read_stmt, *begin_trans_stmt, *end_trans_stmt;

  char *read_str = "SELECT * FROM test WHERE key = ?";
  char *begin_trans_str = "BEGIN TRANSACTION";
  char *end_trans_str = "END TRANSACTION";

  /* Preparing sqlite3 statements */
  status = sqlite3_prepare_v2(db_, begin_trans_str, -1,
                              &begin_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, end_trans_str, -1,
                              &end_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, read_str, -1,
                              &read_stmt, NULL);
  error_check(status);

  bool transaction = (entries_per_batch > 1);
  for (int i = 0; i < reads_; i += entries_per_batch) {
#ifdef SQLITE_OS_QUEC_RTOS
    ql_dev_feed_wdt();
#endif
    /* Begin read transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(begin_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(begin_trans_stmt);
      error_check(status);
    }

    /* Create and execute SQL statements */
    for (int j = 0; j < entries_per_batch; j++) {

#ifdef SQLITE_OS_QUEC_RTOS
      ql_dev_feed_wdt();
#endif
      /* Create key value */
      char key[100];
      int k = (order == SEQUENTIAL) ? i + j : (rand_next(&rand_) % reads_);
      snprintf(key, sizeof(key), "%016d", k);

      /* Bind key value into read_stmt */
      status = sqlite3_bind_blob(read_stmt, 1, key, 16, SQLITE_TRANSIENT);
      error_check(status);
      
      /* Execute read statement */
      while ((status = sqlite3_step(read_stmt)) == SQLITE_ROW) {}
      step_error_check(status);

      /* Reset SQLite statement for another use */
      status = sqlite3_clear_bindings(read_stmt);
      error_check(status);
      status = sqlite3_reset(read_stmt);
      error_check(status);
      finished_single_op();
    }

    /* End read transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(end_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(end_trans_stmt);
      error_check(status);
    }
  }

  status = sqlite3_finalize(read_stmt);
  error_check(status);
  status = sqlite3_finalize(begin_trans_stmt);
  error_check(status);
  status = sqlite3_finalize(end_trans_stmt);
  error_check(status);
}
