#! /usr/bin/perl

die unless chdir( "$ENV{srcdir}/" );
open DIRLIST, "editor-plugins/plugins.dir.list";
while ( <DIRLIST> )
{
    s/#.*\n?$//; next if /^\s*$/; s/\s*$//; s/^\s*//; push @direntry, $_ if length;    
}

open MAKEFILE, "> configure.in";
print MAKEFILE << 'EOF';
dnl This is a generated file.  Please see makeconfig.pl.
AC_INIT(src/gedit.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(gedit, 0.5.3pre)

AM_MAINTAINER_MODE
AM_ACLOCAL_INCLUDE(macros)

GNOME_INIT

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_ARG_PROGRAM
AM_PROG_LIBTOOL


GNOME_X_CHECKS


AM_CONDITIONAL(FALSE, test foo = bar)

	    
dnl Let the user enable the new GModule Plugins
AC_ARG_WITH(gmodule-plugins,
	    [  --with-gmodule-plugins  Enable GModule Plugins [default=no]],
	    enable_gmodule_plugins="$withval", enable_gmodule_plugins=no)

dnl We need to have GNOME to use them
have_gmodule_plugins=no
  if test x$enable_gmodule_plugins = xyes ; then
    AC_DEFINE(WITH_GMODULE_PLUGINS)
    have_gmodule_plugins=yes
  fi

AM_CONDITIONAL(WITH_GMODULE_PLUGINS, test x$have_gmodule_plugins = xyes)

dnl Check for libzvt from gnome-libs/zvt
AC_CHECK_LIB(zvt, zvt_term_new, have_libzvt=yes, have_libzvt=no, $GNOMEUI_LIBS)
AM_CONDITIONAL(HAVE_LIBZVT, test x$have_libzvt = xyes)


ALL_LINGUAS="cs de es fr ga it ko no pt ru sv nl"
AM_GNU_GETTEXT


AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

AC_OUTPUT([
gedit.spec
Makefile
intl/Makefile
po/Makefile.in
macros/Makefile
help/Makefile
help/C/Makefile
help/no/Makefile
src/Makefile
gmodule-plugins/Makefile
gmodule-plugins/idl/Makefile
gmodule-plugins/goad/Makefile
gmodule-plugins/client/Makefile
gmodule-plugins/shell/Makefile
gmodule-plugins/launcher/Makefile
editor-plugins/Makefile
EOF

if ( $#direntry + 1 > 0 )
{
    foreach $i (@direntry)
    {
	print MAKEFILE "editor-plugins/$i/Makefile\n";
    }
}

print MAKEFILE << "EOF";
],[sed -e "/POTFILES =/r po/POTFILES" po/Makefile.in > po/Makefile])
EOF
