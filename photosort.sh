#!/bin/bash

# ^^^^^^^^^^^^^^^^^ photosort.sh ^^^^^^^^^^^^^^^^^
# Copy photos from an external drive and sort them
# by date into a predetermined folder in the computer.
#
# This program takes as input a .txt file path on which the source folder and
# the photo file names are written. Executing photosort.sh automatically creates
# the required .txt file, then runs the compiled version of photosort.c.

# TODO:
# When I adapt the c script to use the given src_dir parameter in its internal logic,
# I'll just have to uncomment the "TODO" lines and delete the "FIXME" lines 


# Parse arguments (if any)
usage() {
    echo "Usage: $0 [-r] ([-s source] [-d destination]) [--help]"
}
remove=""
dest_dir="" # Default destination (resolved in c script)
src_dir=""  # Default source: /media/$USER/$device_dir/DCIM/$photo_dir (printed to tmp)

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -r)
            remove="-r"
            shift
            ;;
        -s|--source)
            src_dir="$2"
            shift 2
            ;;
        -d|--dest)
            dest_dir="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done


# Save current directory path
start_dir=$(dirname "$0")
c_file_name="photosort"
cf="$start_dir/$c_file_name.c"


# Print print values to tmp
tmp_dir="$start_dir/tmp.txt"
photos=""
if [[ -z "$src_dir" ]]; then
    # Access the contents of the SD Card
    device_dir=$(ls /media/$USER)
    photo_dir=$(ls /media/$USER/$device_dir/DCIM) # TODO: need to make it more flexible?
    photos=$(ls /media/$USER/$device_dir/DCIM/$photo_dir)

    ##src_dir directly to tmp #FIXME: remove
    echo "/media/$USER/$device_dir/DCIM/$photo_dir" > "$tmp_dir" # default

    # src_dir passed to c script as argument #TODO: uncomment
    #src_dir="/media/$USER/$device_dir/DCIM/$photo_dir" # default
else
    photos=$(ls "$src_dir")

    ##src_dir directly to tmp #FIXME: remove
    echo "$src_dir" > "$tmp_dir"    

    ## src_dir passed to c script as argument #TODO 
    ##nothing (src_dir is just passed on as an argument)
fi

echo "$photos" >> "$tmp_dir"


# Execute c program (with path to tmp file and arguments)
gcc $cf -o $c_file_name #debug
./$c_file_name $tmp_dir $remove "-d" $dest_dir "-s" $src_dir