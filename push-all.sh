#!/bin/sh

#  push-all.sh
#  CAD Taylor
#
#  Created by Peter Homberg on 3/18/26.
#  
# Konfiguration für HTTP/1.1 und größeren Buffer
git config --global http.version HTTP/1.1
git config --global http.postBuffer 524288000

# Alle Branches pushen
git push --all origin

echo "✅ Alle Branches erfolgreich gepusht!"
