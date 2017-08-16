/*
 * terark_zip_table.cc
 *
 *  Created on: 2016-08-09
 *      Author: leipeng
 */

// project headers
#include "terark_zip_table.h"
#include "terark_zip_index.h"
#include "terark_zip_common.h"
#include "terark_zip_internal.h"
#include "terark_zip_table_reader.h"

// std headers
#include <future>
#include <random>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <util/arena.h> // for #include <sys/mman.h>
#ifdef _MSC_VER
# include <io.h>
#else
# include <sys/types.h>
# include <sys/mman.h>
#endif

// boost headers
#include <boost/predef/other/endian.h>

// rocksdb headers
#include <table/meta_blocks.h>

// terark headers
#include <terark/lcast.hpp>

// 3rd-party headers

static std::once_flag PrintVersionHashInfoFlag;

#ifndef _MSC_VER
const char* git_version_hash_info_core();
const char* git_version_hash_info_fsa();
const char* git_version_hash_info_zbs();
const char* git_version_hash_info_terark_zip_rocksdb();
#endif

void PrintVersionHashInfo(rocksdb::Logger* info_log) {
}



#ifdef TERARK_SUPPORT_UINT64_COMPARATOR
# if !BOOST_ENDIAN_LITTLE_BYTE && !BOOST_ENDIAN_BIG_BYTE
#   error Unsupported endian !
# endif
#endif

namespace rocksdb {

terark::profiling g_pf;

const uint64_t kTerarkZipTableMagicNumber = 0x1122334455667788;

const std::string kTerarkZipTableIndexBlock        = "TerarkZipTableIndexBlock";
const std::string kTerarkZipTableValueTypeBlock    = "TerarkZipTableValueTypeBlock";
const std::string kTerarkZipTableValueDictBlock    = "TerarkZipTableValueDictBlock";
const std::string kTerarkZipTableOffsetBlock       = "TerarkZipTableOffsetBlock";
const std::string kTerarkZipTableCommonPrefixBlock = "TerarkZipTableCommonPrefixBlock";
const std::string kTerarkEmptyTableKey             = "ThisIsAnEmptyTable";

const std::string kTerarkZipTableBuildTimestamp = "terark.build.timestamp";


const size_t CollectInfo::queue_size = 8;
const double CollectInfo::hard_ratio = 0.9;

void CollectInfo::update(uint64_t timestamp
  , size_t raw_value, size_t zip_value
  , size_t raw_store, size_t zip_store) {
  std::unique_lock<std::mutex> l(mutex);
  raw_value_size += raw_value;
  zip_value_size += zip_value;
  raw_store_size += raw_store;
  zip_store_size += zip_store;
  auto comp = [](const CompressionInfo& l, const CompressionInfo& r) {
    return l.timestamp > r.timestamp;
  };
  queue.emplace_back(CompressionInfo{timestamp
    , raw_value, zip_value, raw_store, zip_store});
  std::push_heap(queue.begin(), queue.end(), comp);
  while (queue.size() > queue_size) {
    auto& front = queue.front();
    raw_value_size += front.raw_value;
    zip_value_size += front.zip_value;
    raw_store_size += front.raw_store;
    zip_store_size += front.zip_store;
    std::pop_heap(queue.begin(), queue.end(), comp);
    queue.pop_back();
  }
  estimate_compression_ratio = float(zip_store_size) / float(raw_store_size);
}

bool CollectInfo::hard(size_t raw, size_t zip) {
  return double(zip) / double(raw) > hard_ratio;
}

bool CollectInfo::hard() const {
  std::unique_lock<std::mutex> l(mutex);
  return !queue.empty() && hard(raw_value_size, zip_value_size);
}

float CollectInfo::estimate(float def_value) const {
  float ret = estimate_compression_ratio;
  return ret ? ret : def_value;
}


class TableFactory*
  NewTerarkZipTableFactory(const TerarkZipTableOptions& tzto,
    class TableFactory* fallback) {
  TerarkZipTableFactory* factory = new TerarkZipTableFactory(tzto, fallback);
  if (tzto.debugLevel < 0) {
    STD_INFO("NewTerarkZipTableFactory(\n%s)\n",
      factory->GetPrintableTableOptions().c_str()
    );
  }
  return factory;
}

inline static
bool IsBytewiseComparator(const Comparator* cmp) {
#if 1
  const fstring name = cmp->Name();
  if (name.startsWith("RocksDB_SE_")) {
    return true;
  }
  if (name.startsWith("rev:RocksDB_SE_")) {
    // reverse bytewise compare, needs reverse in iterator
    return true;
  }
# if defined(TERARK_SUPPORT_UINT64_COMPARATOR)
  if (name == "rocksdb.Uint64Comparator") {
    return true;
  }
# endif
  return name == "leveldb.BytewiseComparator";
#else
  return BytewiseComparator() == cmp;
#endif
}


Status
TerarkZipTableFactory::NewTableReader(
  const TableReaderOptions& table_reader_options,
  unique_ptr<RandomAccessFileReader>&& file,
  uint64_t file_size, unique_ptr<TableReader>* table,
  bool prefetch_index_and_filter_in_cache)
  const {
  PrintVersionHashInfo(table_reader_options.ioptions.info_log);
  auto userCmp = table_reader_options.internal_comparator.user_comparator();
  if (!IsBytewiseComparator(userCmp)) {
    return Status::InvalidArgument("TerarkZipTableFactory::NewTableReader()",
      "user comparator must be 'leveldb.BytewiseComparator'");
  }
  Footer footer;
  Status s = ReadFooterFromFile(file.get(), file_size, &footer);
  if (!s.ok()) {
    return s;
  }
  if (footer.table_magic_number() != kTerarkZipTableMagicNumber) {
    if (adaptive_factory_) {
      // just for open table
      return adaptive_factory_->NewTableReader(table_reader_options,
        std::move(file), file_size, table,
        prefetch_index_and_filter_in_cache);
    }
    if (fallback_factory_) {
      return fallback_factory_->NewTableReader(table_reader_options,
        std::move(file), file_size, table,
        prefetch_index_and_filter_in_cache);
    }
    return Status::InvalidArgument(
      "TerarkZipTableFactory::NewTableReader()",
      "fallback_factory is null and magic_number is not kTerarkZipTable"
    );
  }
#if 0
  if (!prefetch_index_and_filter_in_cache) {
    WARN(table_reader_options.ioptions.info_log
      , "TerarkZipTableFactory::NewTableReader(): "
      "prefetch_index_and_filter_in_cache = false is ignored, "
      "all index and data will be loaded in memory\n");
  }
#endif
  BlockContents emptyTableBC;
  s = ReadMetaBlock(file.get(), file_size, kTerarkZipTableMagicNumber
    , table_reader_options.ioptions, kTerarkEmptyTableKey, &emptyTableBC);
  if (s.ok()) {
    std::unique_ptr<TerarkEmptyTableReader>
      t(new TerarkEmptyTableReader(this, table_reader_options));
    s = t->Open(file.release(), file_size);
    if (!s.ok()) {
      return s;
    }
    *table = std::move(t);
    return s;
  }
  std::unique_ptr<TerarkZipTableReader>
    t(new TerarkZipTableReader(this, table_reader_options, table_options_));
  s = t->Open(file.release(), file_size);
  if (s.ok()) {
    *table = std::move(t);
  }
  return s;
}

// defined in terark_zip_table_builder.cc
extern
TableBuilder*
createTerarkZipTableBuilder(const TerarkZipTableFactory* table_factory,
                            const TerarkZipTableOptions& tzo,
                            const TableBuilderOptions&   tbo,
                            uint32_t                     column_family_id,
                            WritableFileWriter*          file,
                            size_t                       key_prefixLen);
extern long long g_lastTime;

TableBuilder*
TerarkZipTableFactory::NewTableBuilder(
  const TableBuilderOptions& table_builder_options,
  uint32_t column_family_id,
  WritableFileWriter* file)
  const {
  PrintVersionHashInfo(table_builder_options.ioptions.info_log);
  auto userCmp = table_builder_options.internal_comparator.user_comparator();
  if (!IsBytewiseComparator(userCmp)) {
    THROW_STD(invalid_argument,
      "TerarkZipTableFactory::NewTableBuilder(): "
      "user comparator must be 'leveldb.BytewiseComparator'");
  }
  int curlevel = table_builder_options.level;
  int numlevel = table_builder_options.ioptions.num_levels;
  int minlevel = table_options_.terarkZipMinLevel;
  if (minlevel < 0) {
    minlevel = numlevel - 1;
  }
  size_t keyPrefixLen = 0;
#if defined(TERARK_SUPPORT_UINT64_COMPARATOR) && BOOST_ENDIAN_LITTLE_BYTE
  if (fstring(userCmp->Name()) == "rocksdb.Uint64Comparator") {
    keyPrefixLen = 0;
  }
#endif
#if 1
  INFO(table_builder_options.ioptions.info_log
    , "nth_newtable{ terark = %3zd fallback = %3zd } curlevel = %d minlevel = %d numlevel = %d fallback = %p\n"
    , nth_new_terark_table_, nth_new_fallback_table_, curlevel, minlevel, numlevel, fallback_factory_
  );
#endif
  if (0 == nth_new_terark_table_) {
    g_lastTime = g_pf.now();
  }
  if (fallback_factory_) {
    if (curlevel >= 0 && curlevel < minlevel) {
      nth_new_fallback_table_++;
      TableBuilder* tb = fallback_factory_->NewTableBuilder(table_builder_options,
        column_family_id, file);
      INFO(table_builder_options.ioptions.info_log
        , "TerarkZipTableFactory::NewTableBuilder() returns class: %s\n"
        , ClassName(*tb).c_str());
      return tb;
    }
  }
  nth_new_terark_table_++;

  return createTerarkZipTableBuilder(
    this,
    table_options_,
    table_builder_options,
    column_family_id,
    file,
    keyPrefixLen);
}

std::string TerarkZipTableFactory::GetPrintableTableOptions() const {
  std::string ret;
  ret.reserve(2000);
  const char* cvb[] = {"false", "true"};
  const int kBufferSize = 200;
  char buffer[kBufferSize];
  const auto& tzto = table_options_;
  const double gb = 1ull << 30;

  ret += "localTempDir             : ";
  ret += tzto.localTempDir;
  ret += '\n';

#ifdef M_APPEND
# error WTF ?
#endif
#define M_APPEND(fmt, value) \
ret.append(buffer, snprintf(buffer, kBufferSize, fmt "\n", value))

  M_APPEND("extendedConfigFile       : %s", tzto.extendedConfigFile.c_str());
  M_APPEND("indexType                : %s", tzto.indexType.c_str());
  M_APPEND("checksumLevel            : %d", tzto.checksumLevel);
  M_APPEND("entropyAlgo              : %d", (int)tzto.entropyAlgo);
  M_APPEND("indexNestLevel           : %d", tzto.indexNestLevel);
  M_APPEND("indexNestScale           : %d", (int)tzto.indexNestScale);
  M_APPEND("indexTempLevel           : %d", (int)tzto.indexTempLevel);
  M_APPEND("terarkZipMinLevel        : %d", tzto.terarkZipMinLevel);
  M_APPEND("minDictZipValueSize      : %zd", tzto.minDictZipValueSize);
  M_APPEND("keyPrefixLen             : %zd", tzto.keyPrefixLen);
  M_APPEND("debugLevel               : %d", (int)tzto.debugLevel);
  M_APPEND("adviseRandomRead         : %s", cvb[!!tzto.adviseRandomRead]);
  M_APPEND("enableCompressionProbe   : %s", cvb[!!tzto.enableCompressionProbe]);
  M_APPEND("useSuffixArrayLocalMatch : %s", cvb[!!tzto.useSuffixArrayLocalMatch]);
  M_APPEND("warmUpIndexOnOpen        : %s", cvb[!!tzto.warmUpIndexOnOpen]);
  M_APPEND("warmUpValueOnOpen        : %s", cvb[!!tzto.warmUpValueOnOpen]);
  M_APPEND("disableSecondPassIter    : %s", cvb[!!tzto.disableSecondPassIter]);
  M_APPEND("offsetArrayBlockUnits    : %d", (int)tzto.offsetArrayBlockUnits);
  M_APPEND("estimateCompressionRatio : %f", tzto.estimateCompressionRatio);
  M_APPEND("sampleRatio              : %f", tzto.sampleRatio);
  M_APPEND("indexCacheRatio          : %f", tzto.indexCacheRatio);
  M_APPEND("softZipWorkingMemLimit   : %.3fGB", tzto.softZipWorkingMemLimit / gb);
  M_APPEND("hardZipWorkingMemLimit   : %.3fGB", tzto.hardZipWorkingMemLimit / gb);
  M_APPEND("smallTaskMemory          : %.3fGB", tzto.smallTaskMemory / gb);
  M_APPEND("singleIndexMemLimit      : %.3fGB", tzto.singleIndexMemLimit / gb);

#undef M_APPEND

  return ret;
}

Status
TerarkZipTableFactory::SanitizeOptions(const DBOptions& db_opts,
                                       const ColumnFamilyOptions& cf_opts)
const {
  auto table_factory = dynamic_cast<TerarkZipTableFactory*>(cf_opts.table_factory.get());
  assert(table_factory);
  auto& tzto = *reinterpret_cast<const TerarkZipTableOptions*>(table_factory->GetOptions());
  try {
    TempFileDeleteOnClose test;
    test.path = tzto.localTempDir + "/Terark-XXXXXX";
    test.open_temp();
    test.writer << "Terark";
    test.complete_write();
  }
  catch (...) {
    std::string msg = "ERROR: bad localTempDir : " + tzto.localTempDir;
    fprintf(stderr , "%s\n" , msg.c_str());
    return Status::InvalidArgument("TerarkZipTableFactory::SanitizeOptions()", msg);
  }
  if (!IsBytewiseComparator(cf_opts.comparator)) {
    return Status::InvalidArgument("TerarkZipTableFactory::SanitizeOptions()",
      "user comparator must be 'leveldb.BytewiseComparator'");
  }
  auto indexFactory = TerarkIndex::GetFactory(table_options_.indexType);
  if (!indexFactory) {
    std::string msg = "invalid indexType: " + table_options_.indexType;
    return Status::InvalidArgument(msg);
  }
  return Status::OK();
}

} /* namespace rocksdb */
