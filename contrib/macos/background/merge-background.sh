#!/bin/bash

rm background.tiff
tiffutil -cathidpicheck background-1x.png background-2x.png -out background.tiff

