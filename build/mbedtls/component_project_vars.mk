# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/mbedtls/mbedtls_v2/port/include $(IDF_PATH)/components/mbedtls/mbedtls_v2/mbedtls/include $(IDF_PATH)/components/mbedtls/mbedtls_v2/port/include/esp8266
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/mbedtls -lmbedtls
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += $(IDF_PATH)/components/mbedtls/mbedtls_v2/mbedtls
COMPONENT_LIBRARIES += mbedtls
COMPONENT_LDFRAGMENTS += 
component-mbedtls-build: 
