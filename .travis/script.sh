#!/bin/sh

set -e

DIR=$(readlink -enq $(dirname $0))

phpize
./configure --silent
make --silent CFLAGS+="-Wall -Wextra -Wno-unused-parameter -DDEBUG_PHPAA"

phpenv config-add "$DIR/apparmor.ini"
make install

php -i > /dev/null
php -m > /dev/null

(cd $DIR; php "$DIR/index.php" > "$DIR/out.001"; cat "$DIR/out.001"; cmp "$DIR/out.001" "$DIR/expected.001")
