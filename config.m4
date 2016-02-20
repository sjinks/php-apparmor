PHP_ARG_ENABLE(
	[apparmor],
	[whether to enable the "apparmor" extension],
	[  --enable-apparmor       Enable "apparmor" extension support]
)

if test $PHP_APPARMOR != "no"; then
	AC_CHECK_HEADERS([sys/apparmor.h])
	PHP_CHECK_LIBRARY(
		[apparmor],
		[aa_change_hat],
		[
			PHP_ADD_LIBRARY(apparmor, 1, APPARMOR_SHARED_LIBADD)
		],
		[]
	)

	PHP_NEW_EXTENSION(apparmor, [aa.c], $ext_shared, [cgi], [-Wall -std=gnu99 -D_GNU_SOURCE])
	PHP_SUBST(APPARMOR_SHARED_LIBADD)
fi
