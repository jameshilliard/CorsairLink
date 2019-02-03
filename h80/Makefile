# For building for the current running version of Linux
TARGET		:= $(shell uname -r)
# Or specific version
#TARGET		:= 3.12.2
#TARGET		:= 3.11.10
#TARGET		:= 3.10.14
#TARGET		:= 3.4.65
#TARGET		:= 3.0.78

KERNEL_MODULES	:= /lib/modules/$(TARGET)
KERNEL_BUILD	:= $(KERNEL_MODULES)/build
SYSTEM_MAP	:= $(shell if test -r /boot/System.map-$(TARGET) ; then echo /boot/System.map-$(TARGET) ; else echo $(KERNEL_BUILD)/System.map ; fi)

DRIVER1 := h80i
DRIVER2 := clink

# Directory below /lib/modules/$(TARGET)/kernel into which to install
# the module:
MOD_SUBDIR = drivers/hwmon

obj-m	:= $(DRIVER1).o
obj-m	+= $(DRIVER2).o

.PHONY: all install modules modules_install h80i_install clink_install clean

all: modules

# Targets for running make directly in the external module directory:
modules clean:
	@$(MAKE) -C $(KERNEL_BUILD) M=$(CURDIR) $@

install: modules_install

modules_install: h80i_install clink_install

# Backup original module if it exists
h80i_install:
	test -d $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR) || mkdir $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)
	if test -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1).ko -a ! -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1)_original.ko ; \
	then \
		cp -a $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1).ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1)_original.ko ; \
	fi
	cp $(DRIVER1).ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)
	depmod -a -F $(SYSTEM_MAP) $(TARGET)
	echo "Remember to add h80i to /etc/modules to autoload"

# Backup original module if it exists
clink_install:
	test -d $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR) || mkdir $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)
	if test -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2).ko -a ! -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2)_original.ko ; \
	then \
		cp -a $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2).ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2)_original.ko ; \
	fi
	cp $(DRIVER2).ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)
	depmod -a -F $(SYSTEM_MAP) $(TARGET)
	echo "Remember to add clink to /etc/modules to autoload"

uninstall: modules_uninstall

modules_uninstall: h80i_uninstall clink_uninstall

# Restore original module if it exists
h80i_uninstall:
	rm -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1).ko
	if test -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1)_original.ko ; \
	then \
		mv $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1)_original.ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER1).ko ; \
	fi
	depmod -a -F $(SYSTEM_MAP) $(TARGET)

# Restore original module if it exists
clink_uninstall:
	rm -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2).ko
	if test -f $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2)_original.ko ; \
	then \
		mv $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2)_original.ko $(KERNEL_MODULES)/kernel/$(MOD_SUBDIR)/$(DRIVER2).ko ; \
	fi
	depmod -a -F $(SYSTEM_MAP) $(TARGET)
