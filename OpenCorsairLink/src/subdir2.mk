################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../source/CorsairFanInfo.cpp \
../source/CorsairLink.cpp 

OBJS += \
./source/CorsairFanInfo.o \
./source/CorsairLink.o 

CPP_DEPS += \
./source/CorsairFanInfo.d \
./source/CorsairLink.d 


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ../source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -Ihidapi-hidraw -I../headers -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


