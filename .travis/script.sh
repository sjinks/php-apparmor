#!/bin/sh

#set -e

DIR=$(readlink -enq $(dirname $0))

phpize
./configure --silent
make --silent CFLAGS+="-Wall -Wextra -Wno-unused-parameter -DDEBUG_PHP_AA"

phpenv config-add "$DIR/apparmor.ini"
make install

php -i > /dev/null
php -m > /dev/null

php "$DIR/index.php" > "$DIR/out.001"
ls -lha "$DIR/"
cmp "$DIR/out.001" "$DIR/expected.001"
