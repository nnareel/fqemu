/* Automatically generated by create_config - do not modify */
#define TARGET_AARCH64 1
#define TARGET_NAME "aarch64"
#define TARGET_ARM 1
#define CONFIG_SOFTMMU 1
#define TARGET_SUPPORTS_MTTCG 1
#define CONFIG_I386_DIS 1
#define CONFIG_ARM_DIS 1
#if HOST_AARCH64
#ifdef __APPLE__
#define CONFIG_HVF_APPLE_SILICON 1
#else
#define CONFIG_KVM 1
#endif
#define CONFIG_ARM_A64_DIS 1
#endif
