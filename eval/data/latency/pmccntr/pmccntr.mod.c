#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x921b07b1, "__cpu_online_mask" },
	{ 0xf6e4df71, "on_each_cpu_cond_mask" },
	{ 0xc917e655, "debug_smp_processor_id" },
	{ 0x92997ed8, "_printk" },
	{ 0xb4923834, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "4EB97A0B31335591EE76DD6");
