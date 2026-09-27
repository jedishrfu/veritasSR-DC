foreach(component IN ITEMS characterizer evolver coefficients)
    add_executable(ex3_test_${component} test_${component}.cpp)
    target_link_libraries(ex3_test_${component} PRIVATE ex3_core)
    add_test(NAME ex3.${component} COMMAND ex3_test_${component})
    set_tests_properties(ex3.${component} PROPERTIES
        LABELS "ex3;components"
        ENVIRONMENT "OMP_NUM_THREADS=1"
        TIMEOUT 60)
endforeach()
