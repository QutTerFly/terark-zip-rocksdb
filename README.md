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

  ///    < 0 : only last level using terarkZip, this is the default
  ///          this is equivalent to terarkZipMinLevel == num_levels-1
  /// others : use terarkZip when curlevel >= terarkZipMinLevel
  ///          this includes the two special cases:
  ///                   == 0 : all levels using terarkZip
  ///          >= num_levels : all levels using fallback TableFactory
  opt.terarkZipMinLevel = -1;

  auto fallback = NewBlockBasedTableFactory(); // or NewAdaptiveTableFactory();
  auto factory = NewTerarkZipTableFactory(opt, fallback);
  options.table_factory.reset(factory);
  // the best way is to just using TerarkZipTable for top most level
  // because compressing speed of TerarkZipTable is slower than rocksdb
  // block compressed table
```


