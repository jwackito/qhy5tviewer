#!/bin/sh
echo "gcc -o qhy5tviewer qhy5t.c qhy5tviewer.c -lSDL -lpthread -lusb -lcfitsio -lSDL_image"
gcc -o qhy5tviewer qhy5t.c qhy5tviewer.c -lSDL -lpthread -lusb -lcfitsio -lSDL_image
if [ $? -eq 0 ]; then
echo "Done!"
fi
