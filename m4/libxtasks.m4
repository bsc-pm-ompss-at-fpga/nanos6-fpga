#	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)

AC_DEFUN([AC_CHECK_XTASKS],
	[
		AC_ARG_WITH(
			[xtasks],
			[AS_HELP_STRING([--with-xtasks=prefix], [specify the installation prefix of xtasks])],
			[ ac_cv_use_xtasks_prefix=$withval ],
			[ ac_cv_use_xtasks_prefix="" ]
		)

		if test x"${ac_cv_use_xtasks_prefix}" != x"" ; then
			AC_MSG_CHECKING([the xtasks installation prefix])
			AC_MSG_RESULT([${ac_cv_use_xtasks_prefix}])
			xtasks_LIBS="-L${ac_cv_use_xtasks_prefix}/lib -Wl,-rpath,${ac_cv_use_xtasks_prefix}/lib -lxtasks"
			xtasks_CPPFLAGS="-I$ac_cv_use_xtasks_prefix/include"
			ac_use_xtasks=yes
		fi

		if test x"${ac_use_xtasks}" != x"" ; then
			ac_save_CPPFLAGS="${CPPFLAGS}"
			ac_save_LIBS="${LIBS}"

			CPPFLAGS="${CPPFLAGS} ${xtasks_CPPFLAGS}"
			LIBS="${LIBS} ${xtasks_LIBS}"

			AC_CHECK_HEADERS([libxtasks.h])
			AC_CHECK_LIB([xtasks],
				[xtasksCreateTask],
				[
					xtasks_LIBS="${xtasks_LIBS}"
					ac_use_xtasks=yes
				],
				[
					if test x"${ac_cv_use_xtasks_prefix}" != x"" ; then
						AC_MSG_ERROR([xtasks cannot be found.])
					else
						AC_MSG_WARN([xtasks cannot be found.])
					fi
					ac_use_xtasks=no
				]
			)

			CPPFLAGS="${ac_save_CPPFLAGS}"
			LIBS="${ac_save_LIBS}"
		fi

		AM_CONDITIONAL(HAVE_xtasks, test x"${ac_use_xtasks}" = x"yes")

		if test x"${ac_use_xtasks}" = x"yes" ; then
			AC_DEFINE(HAVE_xtasks, [1], [xtasks API is available])
		fi

		AC_SUBST([xtasks_LIBS])
		AC_SUBST([xtasks_CPPFLAGS])
	]
)
