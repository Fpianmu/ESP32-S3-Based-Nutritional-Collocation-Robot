# opus component — IDF wrapper for libopus 1.5.2
file(GLOB_RECURSE OPUS_SRCS celt/*.c silk/*.c silk/float/*.c src/opus.c src/opus_encoder.c src/opus_decoder.c src/opus_multistream.c src/opus_multistream_encoder.c src/opus_multistream_decoder.c src/opus_projection_encoder.c src/opus_projection_decoder.c src/repacketizer.c)
list(FILTER OPUS_SRCS EXCLUDE REGEX ".*/arm/.*")
list(FILTER OPUS_SRCS EXCLUDE REGEX ".*/x86/.*")
list(FILTER OPUS_SRCS EXCLUDE REGEX ".*/(opus_demo|opus_compare|test_|tests)/.*")
idf_component_register(SRCS ${OPUS_SRCS} INCLUDE_DIRS . include celt silk silk/float src PRIV_INCLUDE_DIRS celt silk silk/float src)
target_compile_options(${COMPONENT_LIB} PRIVATE -DHAVE_CONFIG_H -DOPUS_BUILD -DCUSTOM_MODES -DFLOATING_POINT -DUSE_ALLOCA -DVAR_ARRAYS -Wno-all -Wno-extra -Wno-implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
