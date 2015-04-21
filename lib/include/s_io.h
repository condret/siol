#ifndef S_IO_API
#define S_IO_API

#include <sdb.h>

#define S_IO_READ	4
#define S_IO_WRITE	2
#define S_IO_EXEC	1
#define S_IO_RW		S_IO_READ|S_IO_WRITE

#define S_IO_SEEK_SET	0
#define S_IO_SEEK_CUR	1
#define S_IO_SEEK_END	2

#define S_TRUE	1
#define S_FALSE	0

typedef struct s_io_t {
	struct s_io_desc_t *desc;
	ut64 off;
	int va;
	int ff;
	int autofd;
	ut32 map_id;
	SdbList *freed_map_ids;
	SdbList *maps;
	//SdbList *cache;
	Sdb *files;
} SIO;

//SIOCbs instances should be predefined and not generated to prevent memleaks
typedef struct s_io_callbacks_t {
	struct s_io_desc_t *(*open)(SIO *io, const char *uri, int flags, int mode);
	int (*read)(SIO *io, struct s_io_desc_t *desc, ut8 *buf, int len);
	ut64 (*lseek)(SIO *io, struct s_io_desc_t *desc, ut64 offset, int whence);
	int (*write)(SIO *io, struct s_io_desc_t *desc, const ut8 *buf, int len);
	int (*close)(struct s_io_desc_t *desc);
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

//desc.c
int s_io_desc_init (SIO *io);
SIODesc *s_io_desc_new (SIOCbs *cbs, int fd, char *uri, int flags, void *data);
void s_io_desc_free (SIODesc *desc);
int s_io_desc_add (SIO *io, SIODesc *desc);
int s_io_desc_del (SIO *io, int fd);
SIODesc *s_io_desc_get (SIO *io, int fd);
int s_io_desc_use (SIO *io, int fd);
ut64 s_io_desc_seek (SIODesc *desc, ut64 offset, int whence);
ut64 s_io_desc_size (SIODesc *desc);
int s_io_desc_fini (SIO *io);

//map.c
SIOMap *s_io_map_new (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size);
void s_io_map_init (SIO *io);
int s_io_map_exists (SIO *io, SIOMap *map);
int s_io_map_exists_for_id (SIO *io, ut32 id);
SIOMap *s_io_map_add (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size);
int s_io_map_del (SIO *io, ut32 id);
int s_io_map_del_for_fd (SIO *io, int fd);
int s_io_map_priorize (SIO *io, ut32 id);
void s_io_map_cleanup (SIO *io);
void s_io_map_fini (SIO *io);

//io.c
SIO *s_io_new ();
SIO *s_io_init (SIO *io);
SIODesc *s_io_open_nomap (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode);
SIODesc *s_io_open (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode);
SIODesc *s_io_open_at (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode, ut64 at);
int s_io_close (SIO *io, int fd);
int s_io_pread_at (SIO *io, ut64 paddr, ut8 *buf, int len);
int s_io_pwrite_at (SIO *io, ut64 paddr, const ut8 *buf, int len);
int s_io_fini (SIO *io);
void s_io_free (SIO *io);

#endif
