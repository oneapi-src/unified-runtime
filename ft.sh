# add_test([=[enqueue-adapter_level_zero_v2]=] "/home/mateuszpn/pr2532/build/bin/test-enqueue" "--gtest_filter=*Level_Zero*")
# set_tests_properties([=[enqueue-adapter_level_zero_v2]=] PROPERTIES  ENVIRONMENT 
#    "UR_ADAPTERS_FORCE_LOAD=\"/home/mateuszpn/pr2532/build/lib/libur_adapter_level_zero_v2.so.0.12.0\"" 
#    "LABELS "conformance;adapter_level_zero_v2" 
#     WORKING_DIRECTORY "/home/mateuszpn/pr2532/build/test/conformance/enqueue" _BACKTRACE_TRIPLES "/home/mateuszpn/pr2532/test/conformance/CMakeLists.txt;22;add_test;/home/mateuszpn/pr2532/test/conformance/CMakeLists.txt;32;do_add_test;/home/mateuszpn/pr2532/test/conformance/CMakeLists.txt;67;add_test_adapter;/home/mateuszpn/pr2532/test/conformance/CMakeLists.txt;78;add_conformance_test;/home/mateuszpn/pr2532/test/conformance/enqueue/CMakeLists.txt;6;add_conformance_test_with_kernels_environment;/home/mateuszpn/pr2532/test/conformance/enqueue/CMakeLists.txt;0;")

# Set environment variable
export UR_ADAPTERS_FORCE_LOAD="/home/mateuszpn/pr2532/build/lib/libur_adapter_level_zero_v2.so.0.12.0"

# Set working directory
#cd /home/mateuszpn/pr2532/build/test/conformance/$1

# Run the test with the specified filter
#/home/mateuszpn/pr2532/build/bin/test-$1 --gtest_filter=*Level_Zero*

# Set working directory
cd /home/mateuszpn/pr2532/build/test/conformance/enqueue

# Run the test with the specified filter
#/home/mateuszpn/pr2532/build/bin/test-enqueue --gtest_filter=urEnqueueMemBufferMapTestWithParam.MapSignalEvent*Level_Zero*
/home/mateuszpn/pr2532/build/bin/test-enqueue --gtest_filter=*Level_Zero*