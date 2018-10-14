file (GLOB files *.c)

find_package (PkgConfig REQUIRED)
pkg_check_modules (JSONC REQUIRED json-c)
include_directories (
	${JSONC_INCLUDE_DIRS}
	)



add_executable (ChatServer ${files})
use_c99 ()
target_link_libraries (ChatServer ${JSONC_LIBRARIES})
