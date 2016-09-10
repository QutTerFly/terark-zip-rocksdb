# terark-zip-rocksdb
TerarkZipTable for rocksdb is a rocksdb Table implementation.

TerarkZipTable leverate terark-zip algorithm to rocksdb, by using TerarkZipTable,
you can store more(3x+ than snappy) on disk and load more more data into memory,
and greatly improve the reading speed! All data are accessed at memory speed!

## Restrictions

- User comparator is not supported, you should encoding your keys to make the
  byte lexical order on key is your required order
- `EnvOptions::use_mmap_reads` must be `true`, can be set by `DBOptions::allow\_mmap\_reads`

## Using TerarkZipTable

```c++
#include <table/terark_zip_table.h>
/// other includes...

  ///....
  TerarkZipTableOptions opt;

  /// since rocksdb doesn't support multiple pass scan input,
  /// TerarkZipTalbe requires large temporary disk space,
  /// fortunately, localTempDir can be on Hard Disk, we just need sequencial
  /// IO, although SSD is better...
  opt.localTempDir = "/path/to/some/temp/dir"; // default is "/tmp"

  TableFactory* factory = NewTerarkZipTableFactory(opt);
  options.table_factory.reset(factory);
  // the best way is to just using TerarkZipTable for top most level
  // because compressing speed of TerarkZipTable is slower than rocksdb
  // block compressed table
```



