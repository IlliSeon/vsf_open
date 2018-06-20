#include "vsf.h"
#include "vsfvm_common.h"

static struct vsf_dynpool_t vsfvm_ext_pool;

int vsfvm_ext_pool_init(uint16_t pool_size)
{
	vsfvm_ext_pool.item_size = sizeof(struct vsfvm_ext_t);
	vsfvm_ext_pool.pool_size = pool_size;
	vsfvm_ext_pool.pool_num = 0;
	return vsf_dynpool_init(&vsfvm_ext_pool);
}

void vsfvm_ext_pool_fini(void)
{
	vsf_dynpool_fini(&vsfvm_ext_pool);
}

struct vsfvm_ext_t *vsfvm_alloc_ext(void)
{
	return (struct vsfvm_ext_t *)vsf_dynpool_alloc(&vsfvm_ext_pool);
}

void vsfvm_free_ext(struct vsfvm_ext_t *ext)
{
	vsf_dynpool_free(&vsfvm_ext_pool, ext);
}

void vsfvm_free_extlist(struct vsflist_t *list)
{
	vsflist_foreach_next(ext, ext_next, list->next, struct vsfvm_ext_t, list)
		vsfvm_free_ext(ext);
}
