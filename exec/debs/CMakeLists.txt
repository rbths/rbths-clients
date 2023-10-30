set(DEB_TMP_DIR ${CMAKE_BINARY_DIR}/target_deb_)
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(DEB_ARCH "amd64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(DEB_ARCH "arm64")
else()
  set(DEB_ARCH "unknown")
endif()

add_custom_target(journald_iterator_deb
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/rm_mkdir.sh ${DEB_TMP_DIR}
    COMMAND mkdir -p ${DEB_TMP_DIR}/opt/rbths/plugins/log_iterators/
    COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/journald_iterator/* ${DEB_TMP_DIR}/
    COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/journald_iterator/DEBIAN/control | sed 's/{ARCH}/${DEB_ARCH}/g' >  ${DEB_TMP_DIR}/DEBIAN/control
    COMMAND cp ${CMAKE_BINARY_DIR}/libs/log_grabber/plugins/log_iterators/journald/libjournald_iterator.so ${DEB_TMP_DIR}/opt/rbths/plugins/log_iterators/
    COMMAND dpkg-deb --build ${DEB_TMP_DIR} 
    COMMAND mv ${DEB_TMP_DIR}.deb journald_iterator.deb
    DEPENDS journald_iterator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(log_indexer_deb
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/rm_mkdir.sh ${DEB_TMP_DIR}
    COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/log_indexer/* ${DEB_TMP_DIR}/
    COMMAND cp ${CMAKE_BINARY_DIR}/exec/log_indexer/logindexer_server  ${DEB_TMP_DIR}/opt/rbths/bin/
    COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/log_indexer/DEBIAN/control | sed 's/{ARCH}/${DEB_ARCH}/g' >  ${DEB_TMP_DIR}/DEBIAN/control
    COMMAND dpkg-deb --build ${DEB_TMP_DIR} 
    COMMAND mv ${DEB_TMP_DIR}.deb log_indexer.deb
    DEPENDS logindexer_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(all_debs DEPENDS log_indexer_deb journald_iterator_deb)
