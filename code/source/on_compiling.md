# to compile, use g++ 10 or above (for cpp+20 compatibility)
# need to add -lgmp to g++ function - needs to be at the end.

# ie

g++-10 -std=c++20 /ot_source/main.cpp -o OT_cpp -lgmp

# directory contents
# /ot_source
#   unit_tests/
#   utils/
#   main.cpp

