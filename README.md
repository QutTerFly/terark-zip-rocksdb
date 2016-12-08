# terark-zip-rocksdb
TerarkZipTable for rocksdb is a rocksdb Table implementation.

TerarkZipTable leverate terark-zip algorithm to rocksdb, by using TerarkZipTable,
you can store more(3x+ than snappy) on disk and load more more data into memory,
and greatly improve the reading speed! All data are accessed at memory speed!

Set single SST file larger will get better compression, to tune SST file file size,
see [rocksdb tuning guide](https://github.com/facebook/rocksdb/wiki/RocksDB-Tuning-Guide)

## Restrictions

- User comparator is not supported, you should encoding your keys to make the
  byte lexical order on key is your required order
- `EnvOptions::use_mmap_reads` must be `true`, can be set by `DBOptions::allow_mmap_reads`

## Using TerarkZipTable

```c++
#include <table/terark_zip_table.h>
/// other includes...

  ///....
  TerarkZipTableOptions opt;

  /// TerarkZipTable needs to create temp files during compression
  opt.localTempDir = "/path/to/some/temp/dir"; // default is "/tmp"

  /// 0 : check sum nothing
  /// 1 : check sum meta data and index, check on file load
  /// 2 : check sum all data, not check on file load, check on record read
  opt.checksumLevel = 2; // default 1

  ///    < 0 : only last level using terarkZip
  ///          this is equivalent to terarkZipMinLevel == num_levels-1
  /// others : use terarkZip when curlevel >= terarkZipMinLevel
  ///          this includes the two special cases:
  ///                   == 0 : all levels using terarkZip
  ///          >= num_levels : all levels using fallback TableFactory
  /// it shown that set terarkZipMinLevel = 0 is the best choice
  /// if mixed with rocksdb's block compression, the block compressed
  /// table may using too much memory & SSD, which degrades the performance
  opt.terarkZipMinLevel = 0; // default

  /// optional
  opt.softZipWorkingMemLimit = 16ull << 30; // default
  opt.hardZipWorkingMemLimit = 32ull << 30; // default

  /// to let rocksdb compaction algo know the estimate SST file size
  opt.estimateCompressionRatio = 0.2;

  /// the global dictionary size over all value size
  opt.sampleRatio = 0.03;
 
  /// other opt are tricky, just use default

  /// rocksdb options when using terark-zip-rocksdb:

  /// fallback can be NULL
  auto fallback = NewBlockBasedTableFactory(); // or NewAdaptiveTableFactory();
  auto factory = NewTerarkZipTableFactory(opt, fallback);
  options.table_factory.reset(factory);

  /// terark-zip use mmap
  options.allow_mmap_reads = true;

  /// universal compaction reduce write amplification and is more friendly for
  /// large SST file, terark SST is better on larger SST file.
  /// although universal compaction needs 2x SSD space on worst case, but
  /// terark-zip's high compression, the used SSD space is much smaller
  /// than rocksdb's block compression schema
  options.compaction_style = rocksdb::kCompactionStyleUniversal;

  /// larger MemTable yield larger level0 SST file
  /// larger SST file make terark-zip better
  options.write_buffer_size     = 1ull << 30; // 1G
  options.target_file_size_base = 1ull << 30; // 1G

  /// single sst file size on greater levels should be larger
  /// sstfile_size(level[n+1]) = sstfile_size(level[n]) * target_file_size_multiplier
  options.target_file_size_multiplier = 2; // can be larger, such as 3,5,10

  /// turn off rocksdb write slowdown, optional. If write slowdown is enabled
  /// and write was really slow down, you may doubt that terark-zip caused it
  options.level0_slowdown_writes_trigger = 1000;
  options.level0_stop_writes_trigger = 1000;
  options.soft_pending_compaction_bytes_limit = 2ull << 40;
  options.hard_pending_compaction_bytes_limit = 4ull << 40;
```

