#!/bin/bash
# Setup script for Sample2MIDI ThirdParty dependencies

# Initialize RTNeural submodule
echo "Initializing RTNeural submodule..."
git submodule update --init --recursive

# Create ThirdParty directory structure if needed
mkdir -p ThirdParty/RTNeural

# If submodule is empty, clone RTNeural
if [ ! -f "ThirdParty/RTNeural/CMakeLists.txt" ]; then
    echo "Cloning RTNeural..."
    git clone https://github.com/jatinchowdhury18/RTNeural.git ThirdParty/RTNeural
fi

echo "ThirdParty setup complete!"
