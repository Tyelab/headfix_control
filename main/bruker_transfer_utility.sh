#!/bin/bash

# Jeremy Delahanty December 2021
# Assistance from Jorge Aldana, Salk Institute SNL/CNL Labs

echo ""
echo "Bruker File Transfer Utility"
echo ""

# Put back to user the argument supplied
echo "Project: " $1

# TODO: A better way of declaring projects, paths, and where to transfer things
# should be implemented. Or future groups should adopt the same structure for
# where data is placed.

# Declare associative array, basically the same as python dictionary
declare -A projectpaths

# Add key:value pairs for project_name:project_path line by line for valid
# project paths/directories
projectpaths["deryn_fd"]=/drives/x/Deryn/2P_raw_data
projectpaths["specialk_cs"]=/drives/v
projectpaths["specialk_lh"]=/drives/u

transfer_path=${projectpaths[$1]}

# Get today's date for making file that outlines datasets needing conversion
d=$(date "+%Y%m%d")

# Create filename for directories needing conversion as a .txt file
conversion_identifier=${d}_${1}.txt

# Change to the team's raw file directory
cd /drives/e/$1

echo ""
echo "Transferring Configuration Files..."
echo ""

# Check if the project transferring files is Deryn's food_dep project
if [ $1 == "deryn_fd" ]; then

    for file in config/*
        do
            if [[ -f $file ]]; then
                echo "Copying: " $file
                rsync -P $file $transfer_path
            else
                echo "No configuration files found!"
            fi
        done

    for file in video/*
        do
            if [[ -f $file ]]; then
                echo "Copying: " $file
                rsync -P $file $transfer_path
            else
                echo "No video files found!"
            fi
        done

    for file in yoked/*
        do
            if [[ -f $file ]]; then
                echo "Copying: " $file
                rsync -P $file $transfer_path
            else
                echo "No yoked files found!"
            fi
        done

    for directory in zstacks/*
        do
            if [[ -d $directory ]]; then
                echo "Copying: " $directory
                rsync -rP $directory $transfer_path
            else
                echo "No z-stacks found!"
            fi
        done

    for directory in microscopy/*
        do
            if [[ -d $directory ]]; then
                echo "Copying: " $directory
                rsync -rP $directory $transfer_path
            else
                echo "No t-series found!"
            fi
        done

      echo ""
      echo "Transfers complete!"
      echo ""

      echo "File transfer to server complete!" | Mail -s "bruker_transfer_utility" dleduke@salk.edu


else

  for file in config/*
    do
        if [[ -f $file ]]; then
            echo "Copying: " $file
            date=$(echo $file | cut -d '/' -f 2 | cut -d '_' -f 1 )
            subject=$(echo $file | cut -d '/' -f 2 | cut -d '_' -f 2 )
            rsync -P --remove-source-files $file $transfer_path/2p/raw/$subject/$date
        else
            echo "No configuration files found!"
        fi
      done

  echo ""
  echo "Transferring Video Files..."
  echo ""

  for file in video/*
    do
        if [[ -f $file ]]; then
            echo "Copying: " $file
            date=$(echo $file | cut -d '/' -f 2 | cut -d '_' -f 1 )
            subject=$(echo $file | cut -d '/' -f 2 | cut -d '_' -f 2 )
            rsync -P --remove-source-files $file $transfer_path/2p/raw/$subject/$date
        else
            echo "No video files found!"
        fi
    done

  echo ""
  echo "Transferring z-stacks..."
  echo ""

  for directory in zstacks/*
    do
        if [[ -d $directory ]]; then
            echo "Copying: " $directory
            date=$(echo $directory | cut -d '/' -f 2 | cut -d '_' -f 1 )
            subject=$(echo $directory | cut -d '/' -f 2 | cut -d '_' -f 2 )
            rsync -rP --remove-source-files $directory $transfer_path/2p/raw/$subject/$date/zstacks
            # Reference Image directories are in the microscopy directory that has just been rsynced. These need
            # to be removed before the data directory can be removed. So get this directory/any other weird directories
            # present (there shouldn't be any others...) and remove them.
            for ref_directory in $directory/*
                do
                    rmdir $ref_directory
                done
            rmdir $directory
        else
            echo "No z-stacks found!"
        fi
    done

  echo ""
  echo "Transferring t-series..."
  echo ""

  for directory in microscopy/*
    do
        if [[ -d $directory ]]; then
            # Directories on the server don't have a folder for microscopy specifically, only z-stacks do
            # Therefore we make a new variable for adding directories to the raw_conversion file that's
            # generated for microscopy conversions.
            # First, if the directory found is a SingleImage folder generated during a Live Scan, tell
            # the user that it was found and then remove it and its contents. These aren't useful for anything.
            if [[ "$directory" == *"Single"* ]]; then
                echo "Single Image Found, removing: "
                echo $directory
                rm -r $directory
            # Otherwise, copy the directory contents to the server and add the completed conversion to the
            # raw_conversion text file used by beyblade.
            else
                server_dir=$(echo $directory | cut -d '/' -f 2 )
                echo "Copying: " $server_dir
                date=$(echo $directory | cut -d '/' -f 2 | cut -d '_' -f 1 )
                subject=$(echo $directory | cut -d '/' -f 2 | cut -d '_' -f 2 )
                # Echo these particular variables to the raw_conversion text file on the server. This will be used
                # by the bruker_pipeline converter beyblade later.
                echo /snlkt/$1/2p/raw/$subject/$date/$server_dir >> /drives/x/bruker_pipeline/raw_conversion/$conversion_identifier
                rsync -rP --remove-source-files $directory $transfer_path/2p/raw/$subject/$date/
                # Reference Image directories are in the microscopy directory that has just been rsynced. These need
                # to be removed before the data directory can be removed. So get this directory/any other weird directories
                # present (there shouldn't be any others...) and remove them.
                for ref_directory in $directory/*
                    do
                        rmdir $ref_directory
                    done
                # Remove the directory now that the transfer to the server is complete. rmdir only removes directories
                # that are empty, so if something went wrong the contents of the directory are safe.
                rmdir $directory
            fi
        else
            echo "No t-series found!"
        fi
    done

  echo ""
  echo "Transfers complete!"
  echo ""

echo "File transfer to server complete!" | Mail -s "bruker_transfer_utility" acoley@salk.edu
echo "File transfer to server complete!" | Mail -s "bruker_transfer_utility" jdelahanty@salk.edu

fi
