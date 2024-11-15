#!/bin/bash

# Starting from file 9 down to file 1
for i in {9..1}; do
  # Calculate the new file number
  j=$((i + 1))
  # Check if the current file exists
#   mv "$i" "$j"
#   mv "$i.desc" "$j.desc"
    mv "$i.c" "$j.c"
done

echo "Files have been renamed."
