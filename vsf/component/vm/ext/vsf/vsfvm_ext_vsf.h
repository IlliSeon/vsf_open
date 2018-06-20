
#ifndef __VSFVM_EXT_VSF_INCLUDED__
#define __VSFVM_EXT_VSF_INCLUDED__

extern const struct vsfvm_class_t vsfvm_ext_gpio;

extern const struct vsfvm_ext_op_t vsfvm_ext_vsfhal;
extern const struct vsfvm_ext_op_t vsfvm_ext_vsftimer;
extern const struct vsfvm_ext_op_t vsfvm_ext_vsfbuffer;

#ifdef VSFVM_VM
void vsfvm_ext_register_vsf(struct vsfvm_t *vm);
#endif

#ifdef VSFVM_COMPILER
void vsfvmc_ext_register_vsf(struct vsfvmc_t *vmc);
#endif

#endif		// __VSFVM_EXT_VSF_INCLUDED__
