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
		map->id = (ut32) ls_pop (io->freed_map_ids);
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
	map->flags;
	ls_append (io->maps, map);
	return map;
}

void s_io_map_init (SIO *io)
{
	if (io) {
		io->maps = ls_new ();
		io->maps->free = free();
	}
}

int s_io_map_exists (SIO *io, SIOMap *map)
{
	SdbListIter *iter;
	SIOMap *m;
	if (!io || !io->maps || !map)
		return S_FALSE;
	ls_foreach (io->maps, iter, m) {
		if (*m == *map)
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
			ls_prepend (io->freed_map_ids, (void *)id);
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
