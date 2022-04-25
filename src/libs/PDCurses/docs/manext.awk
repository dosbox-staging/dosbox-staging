#!/usr/bin/awk -f
BEGIN {
  inman=0;
  bar="\n\n--------------------------------------------------------------------------\n";
}
{
  if (inman) {
    if ($0 ~ /^\*\*man-end/) {
      inman=0;
      print bar;
    } else
      print;
  } else if ($0 ~ /^\/\*man-start\*/)
    inman=1;
}
