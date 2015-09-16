#include <s_io.h>
#include <sdb.h>
#include <stdlib.h>

SIOMap *s_io_map_new (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size)
{
	SIOMap *map = NULL;
	if (!size || !io || !io->maps || ((UT64_MAX - size + 1) < addr))			//prevent overflow
		return NULL;
	map = R_NEW0 (SIOMap);
	if (io->freed_map_ids) {
		map->id = (ut32)(size_t) ls_pop (io->freed_map_ids);				//revive dead ids to prevent overflows
		if (!io->freed_map_ids->length) {						//and keep ids low number so user don't need to type large numbers
			ls_free (io->freed_map_ids);
			io->freed_map_ids = NULL;						//we are not storing pointers here, so free must be NULL or it will segfault
		}
	} else if (io->map_id != 0xffffffff) {							//part 2 of id-overflow-prevention
		io->map_id++;
		map->id = io->map_id;
	} else {
		free (map);
		return NULL;
	}
	map->fd = fd;
	map->from = addr;
	map->to = addr + size - 1;								//SIOMap describes an interval  of addresses (map->from; map->to)
	map->flags = flags;
	map->delta = delta;
	ls_append (io->maps, map);								//new map lives on the top
	return map;
}

void map_free (SIOMap *map)									//not-public-api
{
	if (map)
		free (map->name);
	free (map);
}

void s_io_map_init (SIO *io)
{
	if (io) {
		io->maps = ls_new ();
		io->maps->free = map_free;
	}
}

//check if a map with exact the same properties exists
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

//check if a map with specified id exists
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

//add new map
SIOMap *s_io_map_add (SIO *io, int fd, int flags, ut64 delta, ut64 addr, ut64 size)
{
	SIODesc *desc = s_io_desc_get (io, fd);							//check if desc exists
	if (desc)
		return s_io_map_new (io, fd, flags & desc->flags, delta, addr, size);		//a map cannot have higher permissions than the desc belonging to it
	return NULL;
}

//gets first map where addr fits in
SIOMap *s_io_map_get (SIO *io, ut64 addr)
{
	SIOMap *map;
	SdbListIter *iter;
	if (!io || !io->maps)
		return NULL;
	ls_foreach (io->maps, iter, map) {
		if ((map->from <= addr) && (map->to >= addr))
			return map;
	}
	return NULL;
}

//deletes a map with specified id
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

//delete all maps with specified fd
int s_io_map_del_for_fd (SIO *io, int fd)
{
	SdbListIter *iter, *ator;
	SIOMap *map;
	int ret = S_FALSE;
	if (!io || !io->maps)
		return ret;
	for (iter = io->maps->head; iter != NULL; iter = ator) {
		ator = iter->n;
		map = iter->data;
		if (!map) {									//this is done in s_io_map_cleanup too, but preventing some segfaults here too won't hurt
			ls_delete (io->maps, iter);
		} else if (map->fd == fd) {
			ret = S_TRUE;								//a map with (map->fd == fd) existed/was found and will be deleted now
			if (!io->freed_map_ids) {
				io->freed_map_ids = ls_new ();
				io->freed_map_ids->free = NULL;
			}
			ls_prepend (io->freed_map_ids, (void *)(size_t)fd);
			ls_delete (io->maps, iter);						//delete iter and map
		}
	}
	return ret;
}

//brings map with specified id to the top of of the list
int s_io_map_priorize (SIO *io, ut32 id)
{
	SdbListIter *iter;
	SIOMap *map;
	if (!io || !io->maps)
		return S_FALSE;
	ls_foreach (io->maps, iter, map) {
		if (map->id == id) {								//search for iter with the correct map
			if (io->maps->head == iter)						//check if map is allready at the top
				return S_TRUE;
			if (iter->n)								//bring iter with correct map to the front
				iter->n->p = iter->p;
			if (iter->p)
				iter->p->n = iter->n;
			if (io->maps->tail == iter)
				io->maps->tail = iter->p;
			io->maps->head->p = iter;
			iter->n = io->maps->head;
			io->maps->head = iter;
			iter->p = NULL;
			return S_TRUE;								//TRUE if the map could be priorized
		}
	}
	return S_FALSE;										//FALSE if not
}

//may fix some inconsistencies in io->maps
void s_io_map_cleanup (SIO *io)
{
	SdbListIter *iter, *ator;
	SIOMap *map;
	if (!io || !io->maps)
		return;
	if (!io->files) {									//remove all maps if no descs exist
		s_io_map_fini (io);
		s_io_map_init (io);
		return;
	}
	for (iter = io->maps->head; iter != NULL; iter = ator) {
		map = iter->data;
		ator = iter->n;
		if (!map) {									//remove iter if the map is a null-ptr, this may fix some segfaults
			ls_delete (io->maps, iter);
		} else if (!s_io_desc_get (io, map->fd)) {					//delete map and iter if no desc exists for map->fd in io->files
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

//checks if (from;to) overlaps with (map->from;map->to)
int s_io_map_is_in_range (SIOMap *map, ut64 from, ut64 to)					//rename pls
{
	if (!map || (to < from))
		return S_FALSE;
	if (map->from <= from && from <= map->to)	return S_TRUE;
	if (map->from <= to && to <= map->to)		return S_TRUE;
	if (map->from > from && to > map->to)		return S_TRUE;
	return S_FALSE;
}

void s_io_map_set_name (SIOMap *map, const char *name)
{
	if (!map || !name)
		return;
	free (map->name);
	map->name = strdup (name);
}

void s_io_map_del_name (SIOMap *map)
{
	if (!map)
		return;
	free (map->name);
	map->name = NULL;
}
