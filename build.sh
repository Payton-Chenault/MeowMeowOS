

# Setup the Cross-Compiler Environment
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# If you pass 'clean' (e.g. ./build.sh clean), it will wipe everything
if [ "$1" == "clean" ]; then
    echo "Cleaning build and bin directories (WARNING: This deletes your formatted disk image!)..."
    make clean
    exit 0
fi

# Build the OS and User Programs
echo "Building MeowMeowOS and injecting user programs..."
make all

# Print helpful instructions
echo ""
echo "================================================="
echo "Build complete!"
echo "NOTE: If this was a fresh build and injection failed,"
echo "      run 'make run', type 'format' in the OS shell,"
echo "      exit QEMU, and run './build.sh' one more time."
echo "================================================="
echo "Enter 'make run' or 'make debug' to start!"