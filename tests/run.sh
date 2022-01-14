#!/bin/sh
echo "Running tests."
echo "Copy to external errors."
./copy_to_external_errors
echo "Copy to external simple."
./copy_to_external_simple
echo "Test 1."
./test1
echo "Write 10 blocks simple."
./write_10_blocks_simple
echo "Write 10 blocks spill."
./write_10_blocks_spill
echo "Write more than 10 blocks simple."
./write_more_than_10_blocks_simple
echo "Write big blocks."
./write_10_blocks_big
