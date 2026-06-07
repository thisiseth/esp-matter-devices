idf_component_register(SRCS "main.c" "../../doorbell_chime/main/audio.c"
                    INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "../../doorbell_chime/main/audio.wav" BINARY)
