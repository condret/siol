#include <s_io.h>
#include <sdb.h>
#include <stdlib.h>

SIOMap *s_io_map_new (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size)
{
	SIOMap *map = NULL;
	if (!io || !io->maps || ((0xffffffffffffffff - size) < addr))
		return NULL;
	map = R_NEW0 (SIOMap);
	if (io->freed_map_ids) {
		map->id = (ut32)(size_t) ls_pop (io->freed_map_ids);
		if (!ls_length (io->freed_map_ids)) {
			ls_free (io->freed_map_ids);
			io->freed_map_ids = NULL;
		}
	} else if (io->map_id != 0xffffffff) {
		io->map_id++;
		map->id = io->map_id;
	} else {
		free (map);
		return NULL;
	}
	map->fd = fd;
	map->from = addr;
	map->to = addr + size;
	map->flags = flags;
	ls_append (io->maps, map);
	return map;
}

void s_io_map_init (SIO *io)
{
	if (io) {
		io->maps = ls_new ();
		io->maps->free = free;
	}
}

int s_io_map_exists (SIO *io, SIOMap *map)
{
	SdbListIter *iter;
	SIOMap *m;
	if (!io || !io->maps || !map)
		return S_FALSE;
	ls_foreach (io->maps, iter, m) {
		if (!memcmp (m, map, sizeof(SIOMap)))
			return S_TRUE;
	}
	return S_FALSE;
}

int s_io_map_exists_for_id (SIO *io, ut32 id)
{
	SdbListIter *iter;
	SIOMap *map;
	if (!io || !io->maps)
		return S_FALSE;
	ls_foreach (io->maps, iter, map) {
		if (map->id == id)
			return S_TRUE;
	}
	return S_FALSE;
}

SIOMap *s_io_map_add (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size)
{
	SIODesc *desc = s_io_desc_get (io, fd);
	if (desc)
		return s_io_map_new (io, fd, flags & desc->flags, delta, addr, size);		//a map cannot have higher permissions than the desc belonging to it
	return NULL;
}

int s_io_map_del (SIO *io, ut32 id)
{
	SdbListIter *iter;
	SIOMap *map;
	if (!io || !io->maps)
		return S_FALSE;
	ls_foreach (io->maps, iter, map) {
		if (map->id == id) {
			ls_delete (io->maps, iter);
			if (!io->freed_map_ids) {
				io->freed_map_ids = ls_new ();
				io->freed_map_ids->free = NULL;
			}
			ls_prepend (io->freed_map_ids, (void *)(size_t)id);
			return S_TRUE;
		}
	}
	return S_FALSE;
}

int s_io_map_priorize (SIO *io, ut32 id)
{
	SdbListIter *iter;
	SIOMap *map;
	if (!io || !io->maps)
		return S_FALSE;
	ls_foreach (io->maps, iter, map) {
		if (map->id == id) {
			if (io->maps->head == iter)
				return S_TRUE;
			if (iter->n)
				iter->n->p = iter->p;
			if (iter->p)
				iter->p->n = iter->n;
			if (io->maps->tail == iter)
				io->maps->tail = iter->p;
			io->maps->head->p = iter;
			iter->n = io->maps->head;
			io->maps->head = iter;
			iter->p = NULL;
			return S_TRUE;
		}
	}
	return S_FALSE;
}

void s_io_map_cleanup (SIO *io)
{
	SdbListIter *iter, *ator;
	SIOMap *map;
	if (!io || !io->maps)
		return;
	if (!io->files) {
		s_io_map_fini (io);
		s_io_map_init (io);
		return;
	}
	for (iter = io->maps->head; iter != NULL; iter = ator) {
		map = iter->data;
		ator = iter->n;
		if (!map) {
			ls_delete (io->maps, iter);
		} else if (!s_io_desc_get (io, map->fd)) {
			if (!io->freed_map_ids) {
				io->freed_map_ids = ls_new ();
				io->freed_map_ids->free = NULL;
			}
			ls_prepend (io->freed_map_ids, (void *)(size_t)map->id);
			ls_delete (io->maps, iter);
		}
	}
}

void s_io_map_fini (SIO *io)
{
	if (!io)
		return;
	if (io->maps)
		ls_free (io->maps);
	io->maps = NULL;
	if (io->freed_map_ids)
		ls_free (io->freed_map_ids);
	io->freed_map_ids = NULL;
	io->map_id = 0;
}
