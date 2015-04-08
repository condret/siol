#include <sdh.h>

#define S_IO_READ	4
#define S_IO_WRITE	2
#define S_IO_EXEC	1
#define S_IO_RW		S_IO_READ|S_IO_WRITE

typedef struct s_io_t {
	SIODesc *desc;
	ut64 off;
	int va;
	int ff;
	int autofd;
	SdbList *maps;
	//SdbList *cache;
	Sdb *files;
} SIO;

typedef struct s_io_callbacks_t {
	SIODesc *(*open)(SIO *io, const char *uri, int flags, int mode);
	int (*read)(SIO *io, SIODesc *desc, ut8 *buf, int len);
	int (*lseek)(SIO *io, SIODesc *desc, ut64 offset, int whence);
	int (*write)(SIO *io, SIODesc *desc, const ut8 *buf, int len);
	int (*close)(SIODesc *desc);
} SIOCbs;

typedef struct s_io_map_t {
	int fd;
	int flags;
	ut32 id;
	ut64 from;
	ut64 to;
	ut64 delta;
} SIOMap;

typedef struct s_io_desc_t {
	int fd;
	int flags;
	char *uri;
	void *data;
	SIOCbs *cbs;
	SIO *io;
} SIODesc;
