ADD_CUSTOM_TARGET (doc-python
    PYTHONPATH=${CMAKE_BINARY_DIR}/src/python sphinx-build -E -b html
                  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/html
                  COMMENT "Building Python API documentation with Sphinx")

