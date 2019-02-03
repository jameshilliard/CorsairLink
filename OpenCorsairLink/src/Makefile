################################################################################
# Rework of crazzy c++ version which should have never been c++
################################################################################


# All of the sources participating in the build are defined here

LIBS      = `pkg-config libusb-1.0 libudev --libs`
INCLUDES ?= -I../hidapi `pkg-config libusb-1.0 --cflags`

C_SRCS += \
	main.c \
	CorsairFanInfo.c \
	../hidapi-0.7.0/linux/hid-libusb.c \
	CorsairLink.c 

OBJS += \
	main.o \
	CorsairFanInfo.o \
	../hidapi-0.7.0/linux/hid-libusb.o \
	CorsairLink.o 

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	cc -I. $(INCLUDES) -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# ifneq ($(MAKECMDGOALS),clean)
# ifneq ($(strip $(C++_DEPS)),)
# -include $(C++_DEPS)
# endif
# ifneq ($(strip $(C_DEPS)),)
# -include $(C_DEPS)
# endif
# ifneq ($(strip $(CC_DEPS)),)
# -include $(CC_DEPS)
# endif
# ifneq ($(strip $(CPP_DEPS)),)
# -include $(CPP_DEPS)
# endif
# ifneq ($(strip $(CXX_DEPS)),)
# -include $(CXX_DEPS)
# endif
# ifneq ($(strip $(C_UPPER_DEPS)),)
# -include $(C_UPPER_DEPS)
# endif
# endif


# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: OpenCorsairLink

# Tool invocations
OpenCorsairLink: $(OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC Linker'
	@echo 'Libs: $(LIBS)'
	gcc  -o "OpenCorsairLink" -pthread $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
# clean:
#	-$(RM) $(OBJS)$(C++_DEPS)$(C_DEPS)$(CC_DEPS)$(CPP_DEPS)$(EXECUTABLES)$(CXX_DEPS)$(C_UPPER_DEPS) OpenCorsairLink
#	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

