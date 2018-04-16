# - Try to find zchunk library
#
#  This will define:
#  ZCK_FOUND - system has zchunk library
#  ZCK_LIBRARIES - Link these to use zchunk library
#
#  Mostly copied from FindGLIB2.cmake
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#  Copyright (c) 2006 Philippe Bernery <philippe.bernery@gmail.com>
#  Copyright (c) 2007 Daniel Gollub <gollub@b1-systems.de>
#  Copyright (c) 2007 Alban Browaeys <prahal@yahoo.com>
#  Copyright (c) 2008 Michael Bell <michael.bell@web.de>
#  Copyright (c) 2008-2009 Bjoern Ricks <bjoern.ricks@googlemail.com>
#  Copyright (c) 2018 Jonathan Dieter <jdieter@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


IF (ZCK_LIBRARIES)
  # in cache already
  SET(ZCK_FOUND TRUE)
ELSE (ZCK_LIBRARIES)

  INCLUDE(FindPkgConfig)

  ## Glib
  IF ( ZCK_FIND_REQUIRED )
    SET( _pkgconfig_REQUIRED "REQUIRED" )
  ELSE ( ZCK_FIND_REQUIRED )
    SET( _pkgconfig_REQUIRED "" )
  ENDIF ( ZCK_FIND_REQUIRED )

  IF ( ZCK_MIN_VERSION )
    PKG_SEARCH_MODULE( ZCK ${_pkgconfig_REQUIRED} zck>=${ZCK_MIN_VERSION} )
  ELSE ( ZCK_MIN_VERSION )
    PKG_SEARCH_MODULE( ZCK ${_pkgconfig_REQUIRED} zck )
  ENDIF ( ZCK_MIN_VERSION ) 

  IF (ZCK_FOUND)
    IF (NOT ZCK_FIND_QUIETLY)
      MESSAGE (STATUS "Found zchunk: ${ZCK_LIBDIR}/lib${ZCK_LIBRARIES} (found version \"${ZCK_VERSION}\")")
    ENDIF (NOT ZCK_FIND_QUIETLY)
  ELSE (ZCK_FOUND)
    IF (ZCK_FIND_REQUIRED)
      MESSAGE (SEND_ERROR "Could not find zchunk")
    ENDIF (ZCK_FIND_REQUIRED)
  ENDIF (ZCK_FOUND)

  MARK_AS_ADVANCED(ZCK_LIBRARIES)

ENDIF (ZCK_LIBRARIES)
