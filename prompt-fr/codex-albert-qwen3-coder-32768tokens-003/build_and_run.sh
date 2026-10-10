#!/bin/bash

echo "Building raytracer with C89 compliance..."

# Compile with strict C89 settings
if cc -std=c89 -pedantic-errors -Wall -Wextra -O2 -lm main.c -o raytracer; then
    echo "Compilation successful!"
    
    # Check if executable exists
    if [ -f "raytracer" ]; then
        echo "Running raytracer..."
        ./raytracer
        
        # Check if output file was created
        if [ -f "out.tga" ]; then
            echo "Raytracer completed successfully!"
            echo "Image saved as out.tga"
            ls -lh out.tga
        else
            echo "ERROR: Output file out.tga was not created"
        fi
    else
        echo "ERROR: Executable raytracer was not created"
    fi
else
    echo "Compilation failed!"
    exit 1
fi
