#!/bin/bash

# ^^^^^^^^^^^^^^^^^ photosort.sh ^^^^^^^^^^^^^^^^^
# Copy photos from an external drive and sort them
# by date into a predetermined folder in the computer.
#
# This program takes as input a .txt file path on which the source folder and
# the photo file names are written. Executing photosort.sh automatically creates
# the required .txt file, then runs the compiled version of photosort.c.

# TODO:
# init option to let the user choose the destination folder 
# Catching command options (--help, -r)


# Save current directory path
start_dir=$(dirname "$0")
c_file_name="photosort"
cf="$start_dir/$c_file_name.c"

# Access the contents of the SD Card
device_dir=$(ls /media/$USER)
photo_dir=$(ls /media/$USER/$device_dir/DCIM) # TODO: need to make it more flexible?
photos=$(ls /media/$USER/$device_dir/DCIM/$photo_dir)

# Print path and photo_names to tmp file in the same directory as ths file
destination="$start_dir/tmp.txt"

echo "/media/$USER/$device_dir/DCIM/$photo_dir" > "$destination"
echo "$photos" >> "$destination"

# Execute c program while providing it with the path to the tmp file
gcc $cf -o $c_file_name #debug
./$c_file_name $destination
