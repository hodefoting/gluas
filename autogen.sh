#!/bin/sh

automake --add-missing
autoreconf || exit 1

if ./configure --enable-maintainer-mode "$@"; then
  echo
  echo "Now type 'make' to compile $PROJECT."
else
  echo
  echo "Configure failed or did not finish!"
  exit 1
fi

