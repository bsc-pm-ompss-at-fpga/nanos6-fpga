#	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)

AC_DEFUN([CHECK_UNDEFINED_SYMBOL_VERIFICATION_FLAGS],
	[
		AC_MSG_CHECKING([if the linker accepts flags to fail on undefined symbols])
		AC_LANG_PUSH(C)
		ac_save_LDFLAGS="${LDFLAGS}"
		LDFLAGS="${LDFLAGS} -Wl,-z,defs"
		AC_LINK_IFELSE(
			[AC_LANG_PROGRAM([[]], [[]])],
			[
				AC_MSG_RESULT([yes])
				LDFLAGS="${ac_save_LDFLAGS}"
				LDFLAGS_NOUNDEFINED="-Wl,-z,defs"
			], [
				AC_MSG_RESULT([no])
				LDFLAGS="${ac_save_LDFLAGS}"
				LDFLAGS_NOUNDEFINED=""
			]
		)
		AC_LANG_POP(C)
		
		AC_SUBST(LDFLAGS_NOUNDEFINED)
	]
)


AC_DEFUN([CHECK_AS_NEEDED_FLAGS],
	[
		AC_ARG_ENABLE(
			[link-as-needed],
			[AS_HELP_STRING([--disable-link-as-needed], [do not link libraries as needed])],
			[], [ enable_link_as_needed=yes ]
		)
		
		if test x"${enable_link_as_needed}" = x"yes" ; then
			AC_MSG_CHECKING([if the linker accepts --as-needed])
			AC_LANG_PUSH(C)
			ac_save_CC="${CC}"
			CC="${CC} -Wl,--as-needed"
			AC_LINK_IFELSE(
				[AC_LANG_PROGRAM([[]], [[]])],
				[
					AC_MSG_RESULT([yes])
					AS_NEEDED_FLAGS="-Wl,--as-needed"
				], [
					AC_MSG_RESULT([no])
					AS_NEEDED_FLAGS=""
				]
			)
			CC="${ac_save_CC}"
			AC_LANG_POP(C)
		else
			AS_NEEDED_FLAGS=""
		fi
		
		AC_SUBST(AS_NEEDED_FLAGS)
	]
)


AC_DEFUN([ADD_AS_NEEDED_SUPPORT_TO_LIBTOOL],
	[
		AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])
		AC_REQUIRE([AC_PROG_SED])
		
		AC_MSG_CHECKING([if ltmain.sh must be patched to support -Wl,--as-needed])
		if grep as-needed "$ac_aux_dir/ltmain.sh" > /dev/null ; then
			AC_MSG_RESULT([no, already patched])
			must_patch_ltmain=no
		else
			AC_MSG_RESULT([yes])
			must_patch_ltmain=yes
		fi
		
		if test x"${must_patch_ltmain}" = x"yes" ; then
			_AS_ECHO_N([Patching ltmain.sh... ])
			
			# (A) Apply some of the patches that Debian introduces to ltmain.sh in order to not link against
			# all the dependencies of our libraries. Thus, reducing the over-linking produced by libtool.
			
			# Concatenate dependency_libs only if link_all_deplibs != no
			case="link) libs=\"\$deplibs %DEPLIBS% \$dependency_libs\" ;;"
			newcase="link) libs=\"\$deplibs %DEPLIBS%\"\n"
			newcase=$newcase"\t\ttest \"X\$link_all_deplibs\" != Xno \&\& libs=\"\$libs \$dependency_libs\" ;;"
			script_1="s/$case/$newcase/"
			
			# In the step of checking convenience libraries, iterate dependency_libs only if libdir is empty
			# The objective here is to move 3 lines down (elif -> fi) until the end of the debplib loop.
			script_2="
				# Match only from '# It is a libtool convenience library' until the end of the next loop
				/# It is a libtool convenience library/,/done/ {
					
					# If the elif block is found, move it to the hold buffer
					/elif/ {
						# Append the next two lines to the pattern buffer
						N
						N
						# Exchange pattern and hold buffers to keep the 3 lines
						x
						# Delete the current pattern buffer which should be blank
						d
					}
					
					# If the end of the loop is found, append the contents of the hold buffer
					/done/ {
						# First, we switch buffers to check if the hold buffer contains our code
						x
						# If the hold buffer contained elif, it means that the elif block was prior to the loop
						/elif/ {
							# Restore the initial pattern
							x
							# Append the hold buffer to the pattern space
							G
						}
						# If the hold buffer was empty, the elif block was not prior and the patch is not needed
						/^$/ {
							# Switch buffers again to restore the initial pattern
							x
						}
					}
				}"
			
			# (B) Libtool has a weird 'feature' in which places all the linker options '-Wl,option' at the
			# end of the command line. Some options, like '--as-needed', need to be placed before the
			# library list to have any effect.
			
			parse_flags="       -Wl,*--as-needed*)\n"
			parse_flags=$parse_flags"        deplibs=\"\$deplibs \$wl--as-needed\"\n"
			parse_flags=$parse_flags"        ;;\n"
			parse_flags=$parse_flags"      -Wl,*--no-as-needed*)\n"
			parse_flags=$parse_flags"        deplibs=\"\$deplibs \$wl--no-as-needed\"\n"
			parse_flags=$parse_flags"        ;;\n"
			
			iter_flags="          -Wl,--as-needed)\n"
			iter_flags=$iter_flags"           if test \"\$linkmode,\$pass\" = \"prog,link\"; then\n"
			iter_flags=$iter_flags"             compile_deplibs=\"\$deplib \$compile_deplibs\"\n"
			iter_flags=$iter_flags"             finalize_deplibs=\"\$deplib \$finalize_deplibs\"\n"
			iter_flags=$iter_flags"          else\n"
			iter_flags=$iter_flags"            deplibs=\"\$deplib \$deplibs\"\n"
			iter_flags=$iter_flags"          fi\n"
			iter_flags=$iter_flags"          continue\n"
			iter_flags=$iter_flags"          ;;\n"
			script_3="
				# Match only from within the func_mode_link() function
				/func_mode_link\s*\(\)/,/^}/ {
					
					# Prepend parsing of -Wl,--as-nedded / -Wl,--no-as-needed
					/-Wl,\*)/ {
						s/.*/$parse_flags&/
					}
					
					# Place flags before the library list
					# We are looking for the loop where -pthread is parsed from \$deplib
					/case \$deplib in/ {
						n
						/-pthread/ {
							s/.*/$iter_flags&/
						}
					}
				}"
			
			# Apply scripts
			if "${SED}" -i -e "$script_1" -e "$script_2" -e "$script_3" "$ac_aux_dir/ltmain.sh" ; then
				AC_MSG_RESULT([done])
			else
				AC_MSG_ERROR([failed])
			fi
		fi
	]
)

