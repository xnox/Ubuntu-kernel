include $(srctree)/$(subst ",,$(CONFIG_MV_HAL_RULES_PATH))
# Objects list
obj-$(CONFIG_MV_INCLUDE_PMU) += mvPmu.o
obj-$(CONFIG_MV_INCLUDE_PMU) += mvPmu_asm.o
obj-$(CONFIG_MV_INCLUDE_PMU) += mvSram.o

AFLAGS_mvPmu_asm.o := -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/plat-orion/mv_hal_drivers/linux_oss -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/plat-orion/mv_hal_drivers/common -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/plat-orion/mv_hal_drivers/mv_hal -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/plat-orion/mv_hal_drivers/mv_hal/sata/CoreDriver/  -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/mach-dove/mv_hal_support/dove_family -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/mach-dove/mv_hal_support -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/mach-dove/mv_hal_support/config -I/home/cndougla/hedley/ubuntu-lucid/debian/build/build-mimas/arch/arm/mach-dove/mv_hal_support/mv_hal_sys -DMV_LINUX -DMV_CPU_LE -DMV_ARM -DLINUX
